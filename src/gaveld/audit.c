#define _GNU_SOURCE
#include "audit.h"
#include "config.h"
#include "db.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

static pthread_t      g_thread;
static volatile int   g_running        = 0;
static volatile int   g_ia_signal      = 0;
static pthread_mutex_t g_mutex         = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_cond          = PTHREAD_COND_INITIALIZER;

/* ── User notification (non-blocking) ────────────────────────────────────── */

static void notify_user(const char *title, const char *content) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
        "termux-notification --title \"gaveld audit: %s\""
        " --content \"%s\" 2>/dev/null",
        title, content);
    int rc = system(cmd);
    if (rc != 0)
        glog("WARN", "AUDIT_NOTIFY failed title=%s rc=%d", title, rc);
    else
        glog("INFO", "AUDIT_NOTIFY title=%s", title);
}

/* ── Check 1: verdict rate ───────────────────────────────────────────────── */

static void check_verdict_rate(time_t now) {
    time_t since = now - AUDIT_FLAG_WINDOW_SEC;

    int total  = db_verdict_count_all(since);
    int jailed = db_verdict_count_by_type("JAILED", since);

    if (total < 5) return;  /* too few to flag */

    int pct = (jailed * 100) / total;
    if (pct > AUDIT_VERDICT_RATE_MAX) {
        glog("WARN", "AUDIT verdict_rate JAILED=%d%% (threshold=%d%%)",
             pct, AUDIT_VERDICT_RATE_MAX);
        db_audit_log_insert(now, "verdict_rate", "FLAG",
                            "JAILED verdict rate exceeded threshold");

        int flags = db_audit_flag_count("verdict_rate", since);
        if (flags >= AUDIT_REPEAT_THRESHOLD)
            notify_user("verdict rate", "High JAILED rate — possible false positives");
    } else {
        db_audit_log_insert(now, "verdict_rate", "OK", "");
    }
}

/* ── Check 2: score inflation ────────────────────────────────────────────── */

static void check_score_inflation(time_t now) {
    time_t since = now - AUDIT_FLAG_WINDOW_SEC;

    db_threat_t rows[512];
    int nrows = 0;
    db_threat_all(rows, 512, &nrows);

    if (nrows == 0) return;

    double total_score = 0.0;
    for (int i = 0; i < nrows; i++)
        total_score += rows[i].score;

    double avg = total_score / nrows;

    if (avg > AUDIT_SCORE_INFLATION) {
        glog("WARN", "AUDIT score_inflation avg=%.2f (threshold=%.2f)",
             avg, AUDIT_SCORE_INFLATION);
        db_audit_log_insert(now, "score_inflation", "FLAG",
                            "Average score exceeds inflation threshold");

        int flags = db_audit_flag_count("score_inflation", since);
        if (flags >= AUDIT_REPEAT_THRESHOLD)
            notify_user("score inflation",
                        "Average threat score elevated — review scoring weights");
    } else {
        db_audit_log_insert(now, "score_inflation", "OK", "");
    }
}

/* ── Check 3: stall detection ────────────────────────────────────────────── */

static void check_stalls(time_t now) {
    /* Cases stuck in PENDING_JUDGEMENT beyond AUDIT_STALL_SEC */
    /* We query through db_threat_all indirectly — a direct SQL query
       on the cases table is cleaner; use db_exec equivalent via scoring_log */
    /* TODO: expose db_case_stale_count(cutoff) in db.h if needed.
       For now, log a reminder that this check needs the DB function. */
    (void)now;
    glog("DEBUG", "AUDIT stall_check — db_case_stale_count() not yet exposed");
}

/* ── Check 4: IA_REFERRAL handling ──────────────────────────────────────── */

static void check_ia_referrals(time_t now) {
    /* Own daemons that were flagged — softer review.
       Query verdicts table for unprocessed IA_REFERRAL entries.
       For now: log them and mark processed. Full IA logic is
       a future extension (scoring anomaly detection per-daemon). */
    time_t since = now - AUDIT_INTERVAL_SEC * 2;
    int refs = db_verdict_count_by_type("IA_REFERRAL", since);
    if (refs > 0) {
        glog("INFO", "AUDIT ia_referral count=%d in last %ds — review logs",
             refs, AUDIT_INTERVAL_SEC * 2);
        db_audit_log_insert(now, "ia_referral", "REVIEWED",
                            "IA_REFERRAL verdicts logged, no enforcement");
    }
}

/* ── Full audit tick ─────────────────────────────────────────────────────── */

static void audit_tick(void) {
    time_t now = time(NULL);
    glog("INFO", "AUDIT tick start");

    check_verdict_rate(now);
    check_score_inflation(now);
    check_stalls(now);
    check_ia_referrals(now);

    glog("INFO", "AUDIT tick complete");
}

/* ── Thread ──────────────────────────────────────────────────────────────── */

static void *audit_thread(void *arg) {
    (void)arg;
    glog("INFO", "audit_thread started — interval=%ds", AUDIT_INTERVAL_SEC);

    while (g_running) {
        /* Sleep in 1s chunks, wake early on IA signal */
        for (int i = 0; i < AUDIT_INTERVAL_SEC && g_running; i++) {
            pthread_mutex_lock(&g_mutex);
            if (g_ia_signal) {
                g_ia_signal = 0;
                pthread_mutex_unlock(&g_mutex);
                glog("INFO", "audit_thread: IA signal received — running early tick");
                break;
            }
            pthread_mutex_unlock(&g_mutex);
            sleep(1);
        }

        if (!g_running) break;
        audit_tick();
    }

    glog("INFO", "audit_thread stopped");
    return NULL;
}

/* ── Lifecycle ───────────────────────────────────────────────────────────── */

int audit_start(void) {
    g_running = 1;
    if (pthread_create(&g_thread, NULL, audit_thread, NULL) != 0) {
        glog("ERROR", "audit_thread pthread_create failed");
        g_running = 0;
        return -1;
    }
    return 0;
}

void audit_stop(void) {
    pthread_mutex_lock(&g_mutex);
    g_running = 0;
    pthread_cond_broadcast(&g_cond);
    pthread_mutex_unlock(&g_mutex);
    pthread_join(g_thread, NULL);
}

void audit_signal_ia_referral(void) {
    pthread_mutex_lock(&g_mutex);
    g_ia_signal = 1;
    pthread_cond_signal(&g_cond);
    pthread_mutex_unlock(&g_mutex);
}
