/*
 * Unit tests for src/gaveld/scorer.c — weight × tier × covariance × recidivism
 * scoring maths, backed by a real (scratch) database.
 */

#include "test_harness.h"
#include "gaveld_test_env.h"
#include "db.h"
#include "scorer.h"

#include <string.h>

static score_result_t score(const char *source, const char *signal,
                            double base_weight) {
    score_result_t r;
    memset(&r, 0, sizeof(r));
    CHECK_INT_EQ(scorer_process(source, signal, base_weight, "unit-test", &r), 0);
    return r;
}

static void seed_threat(const char *source, double s,
                        int prior_jails, int prior_quarantines) {
    db_threat_t rec;
    memset(&rec, 0, sizeof(rec));
    strncpy(rec.source, source, sizeof(rec.source) - 1);
    strncpy(rec.state, "CLEAN", sizeof(rec.state) - 1);
    rec.score = s;
    rec.prior_jails = prior_jails;
    rec.prior_quarantines = prior_quarantines;
    CHECK_INT_EQ(db_threat_upsert(&rec), 0);
}

static void test_table_weight_is_used_when_zero(void) {
    /* ADB_ENABLED = 25, unknown source (tier 1.00), single signal. */
    score_result_t r = score("com.tw.app", "ADB_ENABLED", 0.0);
    CHECK_DBL_EQ(r.prev_score, 0.0);
    CHECK_DBL_EQ(r.new_score, 25.0);
    CHECK_DBL_EQ(r.delta, 25.0);
    CHECK_STR_EQ(r.state, "WATCHED");
    CHECK_STR_EQ(r.source, "com.tw.app");
}

static void test_explicit_weight_overrides_table(void) {
    score_result_t r = score("com.ew.app", "ADB_ENABLED", 40.0);
    CHECK_DBL_EQ(r.new_score, 40.0);
    CHECK_STR_EQ(r.state, "WARNED");
}

static void test_unknown_signal_falls_back_to_ten(void) {
    score_result_t r = score("com.us.app", "NOT_IN_WEIGHTS_TABLE", 0.0);
    CHECK_DBL_EQ(r.new_score, 10.0);
    CHECK_STR_EQ(r.state, "CLEAN");
}

static void test_tier_modifiers_scale_the_addition(void) {
    /* Own daemon: 25 * 0.40 */
    score_result_t own = score("rahzerd", "ADB_ENABLED", 0.0);
    CHECK_DBL_EQ(own.new_score, 10.0);

    /* MIUI/AOSP system package: 25 * 0.60 */
    score_result_t sys = score("com.miui.testpkg", "ADB_ENABLED", 0.0);
    CHECK_DBL_EQ(sys.new_score, 15.0);

    /* Sovereignty-listed app: 25 * 0.15 */
    gt_write_file(SOVEREIGNTY_LIST, "com.sovereign.app\n");
    score_result_t sov = score("com.sovereign.app", "ADB_ENABLED", 0.0);
    CHECK_DBL_EQ(sov.new_score, 3.75);
    unlink(SOVEREIGNTY_LIST);
}

static void test_scores_accumulate_across_signals(void) {
    const char *src = "com.stack.app";
    /* Three distinct low-weight signals; the third crosses the stack bonus. */
    score_result_t a = score(src, "PARTNER_BLOATWARE", 0.0);       /* 10 */
    CHECK_DBL_EQ(a.new_score, 10.0);

    score_result_t b = score(src, "MIUI_OPTIMIZATION_OFF", 0.0);   /* 10 */
    CHECK_DBL_EQ(b.new_score, 20.0);
    CHECK_STR_EQ(b.state, "WATCHED");

    /* 3 distinct signals in the window → COV_STACK_BONUS (1.20) */
    score_result_t c = score(src, "GAME_TURBO_ACTIVE", 0.0);       /* 10 * 1.2 */
    CHECK_DBL_EQ(c.delta, 12.0);
    CHECK_DBL_EQ(c.new_score, 32.0);
    CHECK_DBL_EQ(c.prev_score, 20.0);
}

static void test_covariance_network_plus_cpu(void) {
    const char *src = "com.cov.app";
    score_result_t a = score(src, "NETWORK_ANOMALY", 0.0);  /* not in table → 10 */
    CHECK_DBL_EQ(a.new_score, 10.0);

    /* NETWORK_ANOMALY + CPU_HOG in window → COV_NETWORK_CPU (1.60) */
    score_result_t b = score(src, "CPU_HOG", 0.0);          /* 18 * 1.60 */
    CHECK_DBL_EQ(b.delta, 28.8);
    CHECK_DBL_EQ(b.new_score, 38.8);
}

static void test_integrity_violation_covariance_and_jail_counter(void) {
    const char *src = "com.integrity.app";
    /* INTEGRITY_VIOLATION = 45, covariance 1.80 → 81 → JAILED */
    score_result_t r = score(src, "INTEGRITY_VIOLATION", 0.0);
    CHECK_DBL_EQ(r.new_score, 81.0);
    CHECK_STR_EQ(r.state, "JAILED");
    /* First crossing into JAILED records a prior jail. */
    CHECK_INT_EQ(r.prior_jails, 1);
    CHECK_INT_EQ(r.prior_quarantines, 0);

    /* Already JAILED — the counter must not increment again, and the score
       saturates at MAX_SCORE. */
    score_result_t again = score(src, "INTEGRITY_VIOLATION", 0.0);
    CHECK_DBL_EQ(again.new_score, 100.0);
    CHECK_INT_EQ(again.prior_jails, 1);
}

static void test_recidivism_multipliers(void) {
    seed_threat("com.recid.one", 0.0, 1, 0);
    score_result_t one = score("com.recid.one", "ADB_ENABLED", 0.0);
    CHECK_DBL_EQ(one.new_score, 25.0 * RECID_SINGLE_JAIL);   /* 40 */
    CHECK_STR_EQ(one.state, "WARNED");

    seed_threat("com.recid.two", 0.0, 2, 0);
    score_result_t two = score("com.recid.two", "ADB_ENABLED", 0.0);
    CHECK_DBL_EQ(two.new_score, 25.0 * RECID_MULTI_JAIL);    /* 50 */
    CHECK_STR_EQ(two.state, "QUARANTINED");
    /* Crossing into QUARANTINED for the first time bumps the counter. */
    CHECK_INT_EQ(two.prior_quarantines, 1);

    seed_threat("com.recid.q", 0.0, 0, 1);
    score_result_t q = score("com.recid.q", "ADB_ENABLED", 0.0);
    CHECK_DBL_EQ(q.new_score, 25.0 * RECID_QUARANTINE);      /* 32.5 */
    CHECK_STR_EQ(q.state, "WATCHED");
}

static void test_state_thresholds(void) {
    struct { const char *src; double weight; const char *state; } cases[] = {
        { "com.state.clean",  19.0, "CLEAN"        },
        { "com.state.watch",  20.0, "WATCHED"      },
        { "com.state.warn",   40.0, "WARNED"       },
        { "com.state.quar",   50.0, "QUARANTINED"  },
        { "com.state.ha",     70.0, "HOUSE_ARREST" },
        { "com.state.jail",   80.0, "JAILED"       },
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        score_result_t r = score(cases[i].src, "ADB_ENABLED", cases[i].weight);
        CHECK_MSG(strcmp(r.state, cases[i].state) == 0,
                  "score %.1f → %s, want %s",
                  r.new_score, r.state, cases[i].state);
    }
}

static void test_score_is_persisted(void) {
    score("com.persist.app", "ADB_ENABLED", 30.0);

    db_threat_t rec;
    CHECK_INT_EQ(db_threat_load("com.persist.app", &rec), 1);
    CHECK_DBL_EQ(rec.score, 30.0);
    CHECK_STR_EQ(rec.state, "WATCHED");
    CHECK(rec.last_updated > 0);
}

static void test_format_wire_string(void) {
    score_result_t r;
    memset(&r, 0, sizeof(r));
    strcpy(r.source, "com.fmt.app");
    strcpy(r.state, "WARNED");
    r.new_score = 42.567;
    r.delta = 2.5;

    char buf[128];
    scorer_format(&r, buf, sizeof(buf));
    CHECK_STR_EQ(buf, "com.fmt.app|42.57|WARNED|2.50\n");
}

int main(void) {
    gt_env_init();
    if (db_open() != 0 || db_init_schema() != 0) {
        fprintf(stderr, "test_scorer: cannot open %s\n", DB_PATH);
        return 1;
    }

    RUN_TEST(test_table_weight_is_used_when_zero);
    RUN_TEST(test_explicit_weight_overrides_table);
    RUN_TEST(test_unknown_signal_falls_back_to_ten);
    RUN_TEST(test_tier_modifiers_scale_the_addition);
    RUN_TEST(test_scores_accumulate_across_signals);
    RUN_TEST(test_covariance_network_plus_cpu);
    RUN_TEST(test_integrity_violation_covariance_and_jail_counter);
    RUN_TEST(test_recidivism_multipliers);
    RUN_TEST(test_state_thresholds);
    RUN_TEST(test_score_is_persisted);
    RUN_TEST(test_format_wire_string);

    db_close();
    return test_report();
}
