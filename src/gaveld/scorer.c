#define _GNU_SOURCE
#include "scorer.h"
#include "config.h"
#include "db.h"
#include "log.h"
#include "tier.h"
#include "weights.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>

/* ── Score → state ───────────────────────────────────────────────────────── */

static const char *score_to_state(double score) {
    if (score >= THRESH_JAILED)       return "JAILED";
    if (score >= THRESH_HOUSE_ARREST) return "HOUSE_ARREST";
    if (score >= THRESH_QUARANTINED)  return "QUARANTINED";
    if (score >= THRESH_WARNED)       return "WARNED";
    if (score >= THRESH_WATCHED)      return "WATCHED";
    return "CLEAN";
}

/* ── Recidivism modifier ─────────────────────────────────────────────────── */

static double recidivism_mod(int prior_jails, int prior_quarantines) {
    if (prior_jails >= 2)          return RECID_MULTI_JAIL;
    if (prior_jails >= 1)          return RECID_SINGLE_JAIL;
    if (prior_quarantines >= 1)    return RECID_QUARANTINE;
    return RECID_NONE;
}

/* ── Covariance matrix ───────────────────────────────────────────────────── */

typedef struct {
    char   sigs[16][64];
    int    count;
} scored_sigset_t;

static int sigset_has(const scored_sigset_t *ss, const char *sig) {
    for (int i = 0; i < ss->count; i++)
        if (strcmp(ss->sigs[i], sig) == 0) return 1;
    return 0;
}

static double covariance(const scored_sigset_t *ss, double *stack_bonus) {
    *stack_bonus = (ss->count >= COV_STACK_MIN_SIGNALS)
                   ? COV_STACK_BONUS : 1.00;

    if (sigset_has(ss, "INTEGRITY_VIOLATION"))
        return COV_INTEGRITY_VIOLATION;

    if (sigset_has(ss, "NETWORK_ANOMALY") &&
        (sigset_has(ss, "CPU_HOG") || sigset_has(ss, "CPU_HOG_CRITICAL")))
        return COV_NETWORK_CPU;

    if (sigset_has(ss, "WAKELOCK_ANOMALY") &&
        sigset_has(ss, "NETWORK_ANOMALY"))
        return COV_WAKELOCK_NETWORK;

    if (sigset_has(ss, "THERMAL_CRITICAL") &&
        sigset_has(ss, "CPU_HOG_CRITICAL"))
        return COV_THERMAL_CPU_CRITICAL;

    if ((sigset_has(ss, "THERMAL_CRITICAL") ||
         sigset_has(ss, "THERMAL_WARN")) &&
        sigset_has(ss, "NETWORK_ANOMALY"))
        return COV_THERMAL_NETWORK;

    if (sigset_has(ss, "CPU_THROTTLING") &&
        sigset_has(ss, "WAKELOCK_ANOMALY"))
        return COV_CPU_WAKELOCK;

    return 1.00;
}

/* ── Main scoring function ───────────────────────────────────────────────── */

int scorer_process(const char *source, const char *signal,
                   double base_weight, const char *ctx,
                   score_result_t *out) {
    time_t now = time(NULL);
    time_t window_start = now - SIGNAL_WINDOW_SEC;

    /* Use weights table if caller passes 0 */
    if (base_weight <= 0.0) {
        int w = weight_lookup(signal);
        base_weight = (w > 0) ? (double)w : 10.0;
    }

    /* Load current threat record */
    db_threat_t rec;
    db_threat_load(source, &rec);
    double prev_score = rec.score;

    /* Insert current signal into window BEFORE loading covariance
       so this signal participates in its own window */
    db_signal_insert(source, signal, now);
    db_signal_prune(window_start);

    /* Load distinct signals in window (includes current) */
    scored_sigset_t ss = {0};
    db_signal_distinct(source, window_start, ss.sigs, &ss.count);

    /* Compute modifiers */
    double tier_mod    = tier_modifier(source);
    double stack_bonus = 1.00;
    double cov         = covariance(&ss, &stack_bonus);
    double recid       = recidivism_mod(rec.prior_jails, rec.prior_quarantines);

    /* TODO: user_ctx (foreground state) and situational (thermal) — 1.0 until
       those query paths are wired from ingest context */
    double user_ctx    = 1.00;
    double situational = 1.00;

    double addition = base_weight * user_ctx * situational *
                      cov * stack_bonus * recid * tier_mod;

    double new_score = prev_score + addition;
    if (new_score > MAX_SCORE)   new_score = MAX_SCORE;
    if (new_score < SCORE_FLOOR) new_score = SCORE_FLOOR;

    const char *new_state = score_to_state(new_score);
    double delta = new_score - prev_score;

    /* Update prior counts on new state crossings */
    if (strcmp(new_state, "JAILED") == 0 &&
        strcmp(score_to_state(prev_score), "JAILED") != 0)
        rec.prior_jails++;

    if (strcmp(new_state, "QUARANTINED") == 0 &&
        strcmp(score_to_state(prev_score), "QUARANTINED") != 0)
        rec.prior_quarantines++;

    /* Persist */
    rec.score        = new_score;
    rec.last_updated = now;
    strncpy(rec.state, new_state, sizeof(rec.state)-1);
    db_threat_upsert(&rec);

    /* Scoring log */
    char modifiers[256];
    snprintf(modifiers, sizeof(modifiers),
        "{\"tier\":%.2f,\"cov\":%.2f,\"stack\":%.2f,"
        "\"recid\":%.2f,\"uctx\":%.2f,\"sit\":%.2f}",
        tier_mod, cov, stack_bonus, recid, user_ctx, situational);

    db_scoring_log_insert(now, source, signal, base_weight, addition,
                          prev_score, new_score, new_state, modifiers);

    glog("INFO",
         "src=%s sig=%s base=%.0f add=%.2f prev=%.2f new=%.2f state=%s "
         "tier=%.2f cov=%.2f stack=%.2f recid=%.2f",
         source, signal, base_weight, addition,
         prev_score, new_score, new_state,
         tier_mod, cov, stack_bonus, recid);

    /* Populate result */
    strncpy(out->source, source, sizeof(out->source)-1);
    out->prev_score         = prev_score;
    out->new_score          = new_score;
    out->delta              = delta;
    out->prior_jails        = rec.prior_jails;
    out->prior_quarantines  = rec.prior_quarantines;
    strncpy(out->state, new_state, sizeof(out->state)-1);

    return 0;
}

/* ── Wire format ─────────────────────────────────────────────────────────── */

void scorer_format(const score_result_t *r, char *buf, int len) {
    snprintf(buf, len, "%s|%.2f|%s|%.2f\n",
             r->source, r->new_score, r->state, r->delta);
}
