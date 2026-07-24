/*
 * Unit tests for src/gaveld/db.c — SQLite persistence layer.
 * Runs against a real database created under the scratch BASE_DIR.
 */

#include "test_harness.h"
#include "gaveld_test_env.h"
#include "db.h"

#include <string.h>

static void test_threat_load_defaults(void) {
    db_threat_t rec;
    /* Unknown source: returns 0 (not found) but fills safe defaults. */
    CHECK_INT_EQ(db_threat_load("com.unknown.app", &rec), 0);
    CHECK_STR_EQ(rec.source, "com.unknown.app");
    CHECK_STR_EQ(rec.state, "CLEAN");
    CHECK_DBL_EQ(rec.score, 0.0);
    CHECK_INT_EQ(rec.prior_jails, 0);
    CHECK_INT_EQ(rec.prior_quarantines, 0);
}

static void test_threat_upsert_round_trip(void) {
    db_threat_t rec;
    memset(&rec, 0, sizeof(rec));
    strcpy(rec.source, "com.evil.app");
    strcpy(rec.state, "WARNED");
    rec.score = 42.5;
    rec.prior_jails = 1;
    rec.prior_quarantines = 2;
    rec.last_updated = 1700000000;
    CHECK_INT_EQ(db_threat_upsert(&rec), 0);

    db_threat_t got;
    CHECK_INT_EQ(db_threat_load("com.evil.app", &got), 1);
    CHECK_DBL_EQ(got.score, 42.5);
    CHECK_STR_EQ(got.state, "WARNED");
    CHECK_INT_EQ(got.prior_jails, 1);
    CHECK_INT_EQ(got.prior_quarantines, 2);
    CHECK_INT_EQ((long)got.last_updated, 1700000000L);

    /* Second upsert on the same source updates in place. */
    rec.score = 90.0;
    strcpy(rec.state, "JAILED");
    CHECK_INT_EQ(db_threat_upsert(&rec), 0);
    CHECK_INT_EQ(db_threat_load("com.evil.app", &got), 1);
    CHECK_DBL_EQ(got.score, 90.0);
    CHECK_STR_EQ(got.state, "JAILED");

    db_threat_t all[8];
    int n = 0;
    memset(all, 0, sizeof(all));
    CHECK_INT_EQ(db_threat_all(all, 8, &n), 0);
    CHECK_INT_EQ(n, 1);
    CHECK_STR_EQ(all[0].source, "com.evil.app");
}

static void test_threat_all_respects_max(void) {
    db_threat_t rec;
    memset(&rec, 0, sizeof(rec));
    strcpy(rec.state, "CLEAN");
    for (int i = 0; i < 5; i++) {
        snprintf(rec.source, sizeof(rec.source), "bulk.app%d", i);
        rec.score = i;
        CHECK_INT_EQ(db_threat_upsert(&rec), 0);
    }

    db_threat_t out[3];
    int n = 0;
    memset(out, 0, sizeof(out));
    CHECK_INT_EQ(db_threat_all(out, 3, &n), 0);
    CHECK_INT_EQ(n, 3);
}

static void test_signal_window(void) {
    const char *src = "com.signal.app";
    CHECK_INT_EQ(db_signal_insert(src, "CPU_HOG", 1000), 0);
    CHECK_INT_EQ(db_signal_insert(src, "CPU_HOG", 1010), 0);
    CHECK_INT_EQ(db_signal_insert(src, "DNS_ANOMALY", 1020), 0);
    CHECK_INT_EQ(db_signal_insert("other.app", "MEM_PRESSURE", 1020), 0);

    char sigs[16][64];
    int n = 0;
    memset(sigs, 0, sizeof(sigs));
    CHECK_INT_EQ(db_signal_distinct(src, 900, sigs, &n), 0);
    /* Duplicate CPU_HOG collapses; the other source is excluded. */
    CHECK_INT_EQ(n, 2);

    /* since is exclusive: epoch must be strictly greater. */
    memset(sigs, 0, sizeof(sigs));
    CHECK_INT_EQ(db_signal_distinct(src, 1010, sigs, &n), 0);
    CHECK_INT_EQ(n, 1);
    CHECK_STR_EQ(sigs[0], "DNS_ANOMALY");

    CHECK_INT_EQ(db_signal_active(src, 1010), 1);
    CHECK_INT_EQ(db_signal_active(src, 1020), 0);
    CHECK_INT_EQ(db_signal_active("no.such.app", 0), 0);

    /* Prune drops everything strictly older than the cutoff. */
    CHECK_INT_EQ(db_signal_prune(1015), 0);
    memset(sigs, 0, sizeof(sigs));
    CHECK_INT_EQ(db_signal_distinct(src, 0, sigs, &n), 0);
    CHECK_INT_EQ(n, 1);
    CHECK_STR_EQ(sigs[0], "DNS_ANOMALY");
}

static void test_cases_and_status(void) {
    db_case_t c;
    memset(&c, 0, sizeof(c));
    strcpy(c.case_id, "case_1");
    strcpy(c.source, "com.case.app");
    strcpy(c.signal, "ROOT_DETECTED");
    strcpy(c.context, "ctx");
    strcpy(c.status, "PENDING_JUDGEMENT");
    c.score = 55.0;
    c.created = 1700000000;
    CHECK_INT_EQ(db_case_insert(&c), 0);

    /* Duplicate case_id is ignored, not an error. */
    CHECK_INT_EQ(db_case_insert(&c), 0);

    CHECK_INT_EQ(db_case_update_status("case_1", "DISMISSED"), 0);
    /* Updating a case that does not exist is a no-op, still success. */
    CHECK_INT_EQ(db_case_update_status("case_missing", "DISMISSED"), 0);
}

static void test_verdict_counts(void) {
    db_verdict_t v;
    memset(&v, 0, sizeof(v));
    strcpy(v.case_id, "case_v1");
    strcpy(v.source, "com.verdict.app");
    strcpy(v.verdict, "QUARANTINED");
    strcpy(v.mitre_id, "T1406");
    strcpy(v.mitre_tactic, "defense-evasion");
    v.score = 55.0;
    v.epoch = 2000;
    v.consent_required = 1;
    v.consent_granted = 0;
    CHECK_INT_EQ(db_verdict_insert(&v), 0);

    strcpy(v.case_id, "case_v2");
    strcpy(v.verdict, "JAILED");
    v.epoch = 3000;
    CHECK_INT_EQ(db_verdict_insert(&v), 0);

    strcpy(v.case_id, "case_v3");
    strcpy(v.source, "com.other.app");
    v.epoch = 3000;
    CHECK_INT_EQ(db_verdict_insert(&v), 0);

    CHECK_INT_EQ(db_verdict_count_recent("com.verdict.app", "JAILED", 1000), 1);
    CHECK_INT_EQ(db_verdict_count_recent("com.verdict.app", "JAILED", 3000), 0);
    CHECK_INT_EQ(db_verdict_count_recent("com.verdict.app", "WARNED", 0), 0);
    CHECK_INT_EQ(db_verdict_count_by_type("JAILED", 0), 2);
    CHECK_INT_EQ(db_verdict_count_all(0), 3);
    CHECK_INT_EQ(db_verdict_count_all(2500), 2);

    CHECK_INT_EQ(db_criminal_record_insert("com.verdict.app", "JAILED",
                                           "test", "gaveld", 3000), 0);
    CHECK_INT_EQ(db_scoring_log_insert(3000, "com.verdict.app", "ROOT_DETECTED",
                                       35.0, 35.0, 20.0, 55.0, "QUARANTINED",
                                       "{}"), 0);
}

static void test_consent_queue(void) {
    db_consent_t c;
    memset(&c, 0, sizeof(c));
    strcpy(c.case_id, "case_c1");
    strcpy(c.source, "com.consent.app");
    strcpy(c.verdict, "HOUSE_ARREST");
    strcpy(c.timeout_action, "hold");
    c.score = 75.0;
    c.queued = 5000;
    c.timeout_secs = 1800;
    CHECK_INT_EQ(db_consent_insert(&c), 0);

    strcpy(c.case_id, "case_c2");
    c.queued = 4000;
    CHECK_INT_EQ(db_consent_insert(&c), 0);

    db_consent_t pending[8];
    int n = 0;
    memset(pending, 0, sizeof(pending));
    CHECK_INT_EQ(db_consent_pending(pending, 8, &n), 0);
    CHECK_INT_EQ(n, 2);
    /* Oldest queued first. */
    CHECK_STR_EQ(pending[0].case_id, "case_c2");
    CHECK_STR_EQ(pending[1].case_id, "case_c1");
    CHECK_STR_EQ(pending[0].verdict, "HOUSE_ARREST");
    CHECK_INT_EQ(pending[0].timeout_secs, 1800);
    CHECK_DBL_EQ(pending[0].score, 75.0);

    CHECK_INT_EQ(db_consent_remove("case_c2"), 0);
    memset(pending, 0, sizeof(pending));
    CHECK_INT_EQ(db_consent_pending(pending, 8, &n), 0);
    CHECK_INT_EQ(n, 1);
    CHECK_STR_EQ(pending[0].case_id, "case_c1");
}

static void test_audit_flag_count(void) {
    CHECK_INT_EQ(db_audit_log_insert(6000, "verdict_rate", "FLAG", "30%"), 0);
    CHECK_INT_EQ(db_audit_log_insert(6100, "verdict_rate", "FLAG", "31%"), 0);
    CHECK_INT_EQ(db_audit_log_insert(6200, "verdict_rate", "OK", ""), 0);
    CHECK_INT_EQ(db_audit_log_insert(6200, "score_inflation", "FLAG", ""), 0);

    /* Only FLAG rows for the named check, strictly after `since`. */
    CHECK_INT_EQ(db_audit_flag_count("verdict_rate", 0), 2);
    CHECK_INT_EQ(db_audit_flag_count("verdict_rate", 6000), 1);
    CHECK_INT_EQ(db_audit_flag_count("score_inflation", 0), 1);
    CHECK_INT_EQ(db_audit_flag_count("no_such_check", 0), 0);
}

int main(void) {
    gt_env_init();
    if (db_open() != 0 || db_init_schema() != 0) {
        fprintf(stderr, "test_db: cannot open %s\n", DB_PATH);
        return 1;
    }

    RUN_TEST(test_threat_load_defaults);
    RUN_TEST(test_threat_upsert_round_trip);
    RUN_TEST(test_threat_all_respects_max);
    RUN_TEST(test_signal_window);
    RUN_TEST(test_cases_and_status);
    RUN_TEST(test_verdict_counts);
    RUN_TEST(test_consent_queue);
    RUN_TEST(test_audit_flag_count);

    db_checkpoint();
    db_close();
    return test_report();
}
