#define _GNU_SOURCE
#include "decay.h"
#include "config.h"
#include "db.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

static pthread_t       g_thread;
static volatile int    g_running = 0;

/* ── State rank — for de-escalation logging ──────────────────────────────── */

static int state_rank(const char *state) {
    if (!state)                              return 0;
    if (strcmp(state, "JAILED")       == 0) return 5;
    if (strcmp(state, "HOUSE_ARREST") == 0) return 4;
    if (strcmp(state, "QUARANTINED")  == 0) return 3;
    if (strcmp(state, "WARNED")       == 0) return 2;
    if (strcmp(state, "WATCHED")      == 0) return 1;
    return 0;
}

static const char *score_to_state(double score) {
    if (score >= THRESH_JAILED)       return "JAILED";
    if (score >= THRESH_HOUSE_ARREST) return "HOUSE_ARREST";
    if (score >= THRESH_QUARANTINED)  return "QUARANTINED";
    if (score >= THRESH_WARNED)       return "WARNED";
    if (score >= THRESH_WATCHED)      return "WATCHED";
    return "CLEAN";
}

/* ── Single tick ─────────────────────────────────────────────────────────── */

void decay_tick(void) {
    /* Respect internal_affairs lock */
    FILE *ia = fopen(IA_LOCK, "r");
    if (ia) {
        fclose(ia);
        glog("INFO", "decay_tick skipped — internal_affairs.lock active");
        return;
    }

    time_t now          = time(NULL);
    time_t active_since = now - SIGNAL_WINDOW_SEC;

    /* Load all threat records */
    db_threat_t rows[512];
    int nrows = 0;
    db_threat_all(rows, 512, &nrows);

    int processed = 0;
    int skipped   = 0;

    for (int i = 0; i < nrows; i++) {
        /* Skip if source has active signal in window */
        if (db_signal_active(rows[i].source, active_since)) {
            skipped++;
            continue;
        }

        double new_score = rows[i].score * DECAY_FACTOR;
        if (new_score < 0.5) new_score = 0.0;

        const char *new_state  = score_to_state(new_score);
        int old_rank = state_rank(rows[i].state);
        int new_rank = state_rank(new_state);

        if (new_rank < old_rank)
            glog("INFO", "DE-ESCALATE src=%s %s→%s score=%.2f→%.2f",
                 rows[i].source, rows[i].state, new_state,
                 rows[i].score, new_score);

        /* Write back */
        db_threat_t updated = rows[i];
        updated.score        = new_score;
        updated.last_updated = now;
        strncpy(updated.state, new_state, sizeof(updated.state)-1);
        db_threat_upsert(&updated);

        processed++;
    }

    /* Prune stale signal_window entries */
    db_signal_prune(now - SIGNAL_WINDOW_SEC);

    glog("INFO", "decay_tick complete — processed=%d skipped=%d total=%d",
         processed, skipped, nrows);
}

/* ── Thread ──────────────────────────────────────────────────────────────── */

static void *decay_thread(void *arg) {
    (void)arg;
    glog("INFO", "decay_thread started — interval=%ds factor=%.2f",
         DECAY_INTERVAL_SEC, DECAY_FACTOR);

    while (g_running) {
        /* Sleep in 1s increments so stop signal is responsive */
        for (int i = 0; i < DECAY_INTERVAL_SEC && g_running; i++)
            sleep(1);

        if (!g_running) break;
        decay_tick();
    }

    glog("INFO", "decay_thread stopped");
    return NULL;
}

/* ── Lifecycle ───────────────────────────────────────────────────────────── */

int decay_start(void) {
    g_running = 1;
    if (pthread_create(&g_thread, NULL, decay_thread, NULL) != 0) {
        glog("ERROR", "decay_thread pthread_create failed");
        g_running = 0;
        return -1;
    }
    return 0;
}

void decay_stop(void) {
    g_running = 0;
    pthread_join(g_thread, NULL);
}
