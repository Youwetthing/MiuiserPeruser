/*
 * Unit tests for src/gaveld/cases.c — case assembly and verdict queue routing.
 */

#include "test_harness.h"
#include "gaveld_test_env.h"
#include "cases.h"
#include "db.h"

#include <string.h>

static score_result_t make_result(const char *source, const char *state,
                                  double new_score) {
    score_result_t r;
    memset(&r, 0, sizeof(r));
    strncpy(r.source, source, sizeof(r.source) - 1);
    strncpy(r.state, state, sizeof(r.state) - 1);
    r.new_score = new_score;
    r.prev_score = new_score - 10.0;
    r.delta = 10.0;
    return r;
}

static void test_enforceable_state_is_queued(void) {
    score_result_t r = make_result("com.queued.app", "QUARANTINED", 55.0);
    CHECK_INT_EQ(cases_assemble(&r, "ROOT_DETECTED", "pid=99"), 0);
    CHECK_INT_EQ(cases_queue_depth(), 1);

    case_record_t crec;
    CHECK_INT_EQ(cases_dequeue(&crec), 1);
    CHECK_STR_EQ(crec.source, "com.queued.app");
    CHECK_STR_EQ(crec.signal, "ROOT_DETECTED");
    CHECK_STR_EQ(crec.state, "QUARANTINED");
    CHECK_STR_EQ(crec.ctx, "pid=99");
    CHECK_DBL_EQ(crec.score, 55.0);
    CHECK_DBL_EQ(crec.delta, 10.0);
    CHECK(strncmp(crec.case_id, "case_", 5) == 0);
    CHECK(crec.epoch > 0);
    CHECK_INT_EQ(cases_queue_depth(), 0);
}

static void test_clean_and_watched_are_dismissed(void) {
    score_result_t clean = make_result("com.clean.app", "CLEAN", 5.0);
    CHECK_INT_EQ(cases_assemble(&clean, "EEA_BUILD", NULL), 0);
    CHECK_INT_EQ(cases_queue_depth(), 0);

    score_result_t watched = make_result("com.watched.app", "WATCHED", 25.0);
    CHECK_INT_EQ(cases_assemble(&watched, "ADB_ENABLED", "ctx"), 0);
    CHECK_INT_EQ(cases_queue_depth(), 0);
}

static void test_null_context_is_empty(void) {
    score_result_t r = make_result("com.noctx.app", "WARNED", 45.0);
    CHECK_INT_EQ(cases_assemble(&r, "ADB_ENABLED", NULL), 0);

    case_record_t crec;
    CHECK_INT_EQ(cases_dequeue(&crec), 1);
    CHECK_STR_EQ(crec.ctx, "");
}

static void test_all_containment_states_are_routed(void) {
    const char *states[] = { "WARNED", "QUARANTINED", "HOUSE_ARREST", "JAILED" };
    for (unsigned i = 0; i < sizeof(states) / sizeof(states[0]); i++) {
        score_result_t r = make_result("com.route.app", states[i], 60.0);
        CHECK_INT_EQ(cases_assemble(&r, "ROOT_DETECTED", "ctx"), 0);
    }
    CHECK_INT_EQ(cases_queue_depth(), 4);

    case_record_t crec;
    for (unsigned i = 0; i < sizeof(states) / sizeof(states[0]); i++) {
        CHECK_INT_EQ(cases_dequeue(&crec), 1);
        CHECK_STR_EQ(crec.state, states[i]);   /* FIFO order preserved */
    }
    CHECK_INT_EQ(cases_queue_depth(), 0);
}

static void test_queue_drops_oldest_when_full(void) {
    for (int i = 0; i < VERDICT_QUEUE_SIZE + 1; i++) {
        char src[64];
        snprintf(src, sizeof(src), "com.full.app%d", i);
        score_result_t r = make_result(src, "JAILED", 90.0);
        CHECK_INT_EQ(cases_assemble(&r, "ROOT_DETECTED", "ctx"), 0);
    }
    CHECK_INT_EQ(cases_queue_depth(), VERDICT_QUEUE_SIZE);

    case_record_t crec;
    CHECK_INT_EQ(cases_dequeue(&crec), 1);
    CHECK_STR_EQ(crec.source, "com.full.app1");   /* app0 was evicted */

    while (cases_queue_depth() > 0) cases_dequeue(&crec);
}

/* Must run last: cases_stop() permanently disarms the queue. */
static void test_stop_unblocks_dequeue(void) {
    cases_stop();
    case_record_t crec;
    CHECK_INT_EQ(cases_dequeue(&crec), 0);

    /* Cases assembled after stop are still recorded, and a queued case is
       still drained rather than lost. */
    score_result_t r = make_result("com.late.app", "JAILED", 95.0);
    CHECK_INT_EQ(cases_assemble(&r, "ROOT_DETECTED", "ctx"), 0);
    CHECK_INT_EQ(cases_dequeue(&crec), 1);
    CHECK_STR_EQ(crec.source, "com.late.app");
    CHECK_INT_EQ(cases_dequeue(&crec), 0);
}

int main(void) {
    gt_env_init();
    if (db_open() != 0 || db_init_schema() != 0) {
        fprintf(stderr, "test_cases: cannot open %s\n", DB_PATH);
        return 1;
    }

    RUN_TEST(test_enforceable_state_is_queued);
    RUN_TEST(test_clean_and_watched_are_dismissed);
    RUN_TEST(test_null_context_is_empty);
    RUN_TEST(test_all_containment_states_are_routed);
    RUN_TEST(test_queue_drops_oldest_when_full);
    RUN_TEST(test_stop_unblocks_dequeue);

    db_close();
    return test_report();
}
