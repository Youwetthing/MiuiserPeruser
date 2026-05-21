#define _GNU_SOURCE
#include "verdict.h"
#include "config.h"
#include "cases.h"
#include "consent.h"
#include "enforce.h"
#include "db.h"
#include "log.h"
#include "tier.h"
#include "audit.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

static pthread_t     g_thread;
static volatile int  g_running = 0;

/* ── IA lock ─────────────────────────────────────────────────────────────── */

static int ia_lock_active(void) {
    FILE *fp = fopen(IA_LOCK, "r");
    if (!fp) return 0;
    fclose(fp);
    return 1;
}

/* ── Sovereignty cap ─────────────────────────────────────────────────────── */

/*
 * Sovereignty apps are user-trusted. Hard cap at QUARANTINED.
 * Applies BEFORE ethical floors so floors don't elevate past the cap.
 */
static void apply_sovereignty_cap(const char *source, char *state) {
    if (!tier_is_sovereignty(source)) return;

    if (strcmp(state, "JAILED")       == 0 ||
        strcmp(state, "HOUSE_ARREST") == 0) {
        glog("INFO", "SOVEREIGNTY_CAP src=%s %s→QUARANTINED", source, state);
        strncpy(state, "QUARANTINED", MAX_STATE_LEN - 1);
    }
}

/* ── Ethical floors ──────────────────────────────────────────────────────── */

/*
 * Floor 1: JAILED requires prior HOUSE_ARREST or QUARANTINED.
 * Checks criminal_record and recent verdicts tables.
 * If no prior containment → cap to HOUSE_ARREST.
 */
static void apply_floor_jailed(const char *source, char *state) {
    if (strcmp(state, "JAILED") != 0) return;

    time_t session_start = time(NULL) - (24 * 3600); /* 24h window */

    int prior_ha = db_verdict_count_recent(source, "HOUSE_ARREST", session_start);
    int prior_q  = db_verdict_count_recent(source, "QUARANTINED",  session_start);

    if (prior_ha == 0 && prior_q == 0) {
        glog("INFO",
             "ETHICAL_FLOOR src=%s no prior containment — JAILED→HOUSE_ARREST",
             source);
        strncpy(state, "HOUSE_ARREST", MAX_STATE_LEN - 1);
    }
}

/*
 * Floor 2: no enforcement without a fresh signal in the last 60s.
 * Prevents stale scores from triggering enforcement after decay.
 */
static int floor_has_fresh_signal(const char *source) {
    time_t cutoff = time(NULL) - FLOOR_CLEAN_SIGNAL_MAX_AGE_SEC;
    return db_signal_active(source, cutoff);
}

/* ── Verdict emit ────────────────────────────────────────────────────────── */

void verdict_emit(const char *case_id, const char *source,
                  const char *verdict, double score,
                  int consent_required, int consent_granted) {
    time_t now = time(NULL);

    db_verdict_t v;
    memset(&v, 0, sizeof(v));
    strncpy(v.case_id, case_id, sizeof(v.case_id) - 1);
    strncpy(v.source,  source,  sizeof(v.source)  - 1);
    strncpy(v.verdict, verdict, sizeof(v.verdict) - 1);
    v.score            = score;
    v.epoch            = now;
    v.consent_required = consent_required;
    v.consent_granted  = consent_granted;
    db_verdict_insert(&v);

    /* Criminal record for serious verdicts */
    if (strcmp(verdict, "JAILED")      == 0 ||
        strcmp(verdict, "HOUSE_ARREST") == 0 ||
        strcmp(verdict, "QUARANTINED") == 0) {
        db_criminal_record_insert(source, verdict, "verdict_thread", "gaveld", now);
    }

    db_case_update_status(case_id, verdict);

    glog("INFO", "VERDICT case=%s src=%s verdict=%s score=%.2f consent=%d/%d",
         case_id, source, verdict, score, consent_required, consent_granted);
}

/* ── Core verdict logic ──────────────────────────────────────────────────── */

static void verdict_process(const case_record_t *crec) {
    char state[MAX_STATE_LEN];
    strncpy(state, crec->state, sizeof(state) - 1);

    /* 1. IA lock — suspend entire pipeline */
    if (ia_lock_active()) {
        glog("INFO", "SUSPENDED case=%s src=%s — ia_lock active",
             crec->case_id, crec->source);
        db_case_update_status(crec->case_id, "SUSPENDED");
        return;
    }

    /* 2. Own daemon — route to internal affairs, not regular enforcement */
    if (tier_is_own_daemon(crec->source)) {
        glog("INFO", "IA_REFERRAL src=%s state=%s — routing to audit thread",
             crec->source, state);
        verdict_emit(crec->case_id, crec->source, "IA_REFERRAL",
                     crec->score, 0, 0);
        audit_signal_ia_referral();  /* wake audit thread immediately */
        return;
    }

    /* 3. Fresh signal floor — don't enforce on stale scores */
    if (!floor_has_fresh_signal(crec->source)) {
        glog("INFO", "STALE_SIGNAL src=%s — no signal in last %ds, dismissing",
             crec->source, FLOOR_CLEAN_SIGNAL_MAX_AGE_SEC);
        verdict_emit(crec->case_id, crec->source, "DISMISSED_STALE",
                     crec->score, 0, 0);
        return;
    }

    /* 4. Sovereignty cap — must apply before ethical floors */
    apply_sovereignty_cap(crec->source, state);

    /* 5. Ethical floor — JAILED requires prior containment */
    apply_floor_jailed(crec->source, state);

    /* 6. WARNED — enforce immediately, no consent needed */
    if (strcmp(state, "WARNED") == 0) {
        glog("INFO", "WARNED src=%s score=%.2f — immediate intervene",
             crec->source, crec->score);
        verdict_emit(crec->case_id, crec->source, "WARNED",
                     crec->score, 0, 1);
        enforce_execute(crec->source, state, crec->case_id, crec->score);
        return;
    }

    /* 7. Defensive guard — only known containment states reach consent */
    if (strcmp(state, "QUARANTINED") != 0 &&
        strcmp(state, "HOUSE_ARREST") != 0 &&
        strcmp(state, "JAILED")       != 0) {
        glog("WARN", "verdict_process: unexpected state=%s src=%s — dismissing",
             state, crec->source);
        verdict_emit(crec->case_id, crec->source, "DISMISSED",
                     crec->score, 0, 0);
        return;
    }

    /*
     * 8. QUARANTINED / HOUSE_ARREST / JAILED — async consent gate.
     *    Verdict thread hands off and returns immediately.
     *    consent_thread owns the notification, timeout, and enforce call.
     */
    consent_request_t req;
    memset(&req, 0, sizeof(req));
    strncpy(req.case_id, crec->case_id, sizeof(req.case_id) - 1);
    strncpy(req.source,  crec->source,  sizeof(req.source)  - 1);
    strncpy(req.verdict, state,         sizeof(req.verdict) - 1);
    req.score  = crec->score;
    req.queued = time(NULL);

    /* Timeout policy from config.h */
    if (strcmp(state, "JAILED") == 0) {
        req.timeout_secs   = CONSENT_TIMEOUT_JAILED;      /* 0 = wait indefinitely */
        strncpy(req.timeout_action, "hold", sizeof(req.timeout_action) - 1);
    } else if (strcmp(state, "HOUSE_ARREST") == 0) {
        req.timeout_secs   = CONSENT_TIMEOUT_HOUSE_ARREST;
        strncpy(req.timeout_action, "hold", sizeof(req.timeout_action) - 1);
    } else {
        /* QUARANTINED */
        req.timeout_secs   = CONSENT_TIMEOUT_QUARANTINED;
        strncpy(req.timeout_action, "hold", sizeof(req.timeout_action) - 1);
    }

    /* Record as pending in DB so consent_thread survives a restart */
    db_consent_t dbc;
    memset(&dbc, 0, sizeof(dbc));
    strncpy(dbc.case_id,        req.case_id,        sizeof(dbc.case_id)        - 1);
    strncpy(dbc.source,         req.source,         sizeof(dbc.source)         - 1);
    strncpy(dbc.verdict,        req.verdict,        sizeof(dbc.verdict)        - 1);
    strncpy(dbc.timeout_action, req.timeout_action, sizeof(dbc.timeout_action) - 1);
    dbc.score        = req.score;
    dbc.queued       = req.queued;
    dbc.timeout_secs = req.timeout_secs;
    db_consent_insert(&dbc);

    consent_enqueue(&req);

    glog("INFO",
         "CONSENT_QUEUED case=%s src=%s verdict=%s timeout=%ds",
         crec->case_id, crec->source, state, req.timeout_secs);
}

/* ── Thread ──────────────────────────────────────────────────────────────── */

static void *verdict_thread(void *arg) {
    (void)arg;
    glog("INFO", "verdict_thread started");

    case_record_t crec;
    while (g_running) {
        if (!cases_dequeue(&crec)) break;   /* stopped + empty */
        verdict_process(&crec);
    }

    glog("INFO", "verdict_thread stopped");
    return NULL;
}

/* ── Lifecycle ───────────────────────────────────────────────────────────── */

int verdict_start(void) {
    g_running = 1;
    if (pthread_create(&g_thread, NULL, verdict_thread, NULL) != 0) {
        glog("ERROR", "verdict_thread pthread_create failed");
        g_running = 0;
        return -1;
    }
    return 0;
}

void verdict_stop(void) {
    g_running = 0;
    cases_stop();   /* unblocks cases_dequeue */
    pthread_join(g_thread, NULL);
}
