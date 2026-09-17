/*
 * Unit tests for src/gaveld/ingest.c — wire-format parser and record queue.
 *
 * parse_line() and the ring buffer are file-static, so the implementation is
 * included directly rather than linked; the FIFO thread is never started.
 */

#include "test_harness.h"
#include "gaveld_test_env.h"

#include "ingest.c"

static void test_parse_full_record(void) {
    ingest_record_t r;
    CHECK_INT_EQ(parse_line("com.evil.app|CPU_HOG|18|pid=1234 cpu=91%\n", &r), 1);
    CHECK_INT_EQ(r.is_query, 0);
    CHECK_STR_EQ(r.source, "com.evil.app");
    CHECK_STR_EQ(r.signal, "CPU_HOG");
    CHECK_DBL_EQ(r.weight, 18.0);
    CHECK_STR_EQ(r.ctx, "pid=1234 cpu=91%");
}

static void test_parse_context_keeps_pipes(void) {
    ingest_record_t r;
    CHECK_INT_EQ(parse_line("rahzerd|DNS_ANOMALY|22|host=a|port=53\n", &r), 1);
    CHECK_STR_EQ(r.ctx, "host=a|port=53");
}

static void test_parse_optional_fields(void) {
    ingest_record_t r;

    /* No context. */
    CHECK_INT_EQ(parse_line("src|SIG|30\n", &r), 1);
    CHECK_STR_EQ(r.ctx, "");
    CHECK_DBL_EQ(r.weight, 30.0);

    /* No weight — 0 means "use the weights table". */
    CHECK_INT_EQ(parse_line("src|SIG\n", &r), 1);
    CHECK_DBL_EQ(r.weight, 0.0);
    CHECK_STR_EQ(r.ctx, "");

    /* Non-numeric weight degrades to 0 rather than failing the record. */
    CHECK_INT_EQ(parse_line("src|SIG|abc|ctx\n", &r), 1);
    CHECK_DBL_EQ(r.weight, 0.0);
    CHECK_STR_EQ(r.ctx, "ctx");

    /* Fractional weights survive. */
    CHECK_INT_EQ(parse_line("src|SIG|12.5|ctx\n", &r), 1);
    CHECK_DBL_EQ(r.weight, 12.5);
}

static void test_parse_rejects_malformed(void) {
    ingest_record_t r;
    CHECK_INT_EQ(parse_line("\n", &r), 0);
    CHECK_INT_EQ(parse_line("only_source\n", &r), 0);
    CHECK_INT_EQ(parse_line("|\n", &r), 0);
    CHECK_INT_EQ(parse_line("||\n", &r), 0);
}

/*
 * Known quirk, pinned so a parser rewrite has to be a deliberate decision:
 * parse_line() splits with strtok(), which collapses runs of delimiters, so
 * empty fields are not rejected — they shift every later field left.
 */
static void test_parse_empty_fields_shift_left(void) {
    ingest_record_t r;

    CHECK_INT_EQ(parse_line("|SIG|10|ctx\n", &r), 1);
    CHECK_STR_EQ(r.source, "SIG");
    CHECK_STR_EQ(r.signal, "10");
    CHECK_STR_EQ(r.ctx, "");

    CHECK_INT_EQ(parse_line("src||10|ctx\n", &r), 1);
    CHECK_STR_EQ(r.source, "src");
    CHECK_STR_EQ(r.signal, "10");
    CHECK_STR_EQ(r.ctx, "");
}

static void test_parse_query_records(void) {
    ingest_record_t r;
    CHECK_INT_EQ(parse_line("QUERY|STATUS\n", &r), 1);
    CHECK_INT_EQ(r.is_query, 1);
    CHECK_STR_EQ(r.query_cmd, "STATUS");
    CHECK_STR_EQ(r.source, "");

    CHECK_INT_EQ(parse_line("QUERY|DECAY\n", &r), 1);
    CHECK_STR_EQ(r.query_cmd, "DECAY");

    /* Lower case is not the control prefix — parsed as a signal record. */
    CHECK_INT_EQ(parse_line("query|STATUS|10\n", &r), 1);
    CHECK_INT_EQ(r.is_query, 0);
    CHECK_STR_EQ(r.source, "query");
}

static void test_parse_truncates_oversized_fields(void) {
    char line[BUF_SIZE * 2];
    char big_source[INGEST_SOURCE_MAX * 2];
    memset(big_source, 'a', sizeof(big_source) - 1);
    big_source[sizeof(big_source) - 1] = '\0';
    snprintf(line, sizeof(line), "%s|SIG|10|ctx\n", big_source);

    ingest_record_t r;
    CHECK_INT_EQ(parse_line(line, &r), 1);
    CHECK_INT_EQ((int)strlen(r.source), INGEST_SOURCE_MAX - 1);
    CHECK_INT_EQ(r.source[INGEST_SOURCE_MAX - 1], '\0');
}

static ingest_record_t make_rec(const char *source) {
    ingest_record_t r;
    memset(&r, 0, sizeof(r));
    strncpy(r.source, source, sizeof(r.source) - 1);
    strncpy(r.signal, "CPU_HOG", sizeof(r.signal) - 1);
    return r;
}

static void test_queue_fifo_order(void) {
    CHECK_INT_EQ(ingest_queue_depth(), 0);

    ingest_record_t a = make_rec("first"), b = make_rec("second");
    queue_push(&a);
    queue_push(&b);
    CHECK_INT_EQ(ingest_queue_depth(), 2);

    ingest_record_t out;
    CHECK_INT_EQ(ingest_dequeue(&out), 1);
    CHECK_STR_EQ(out.source, "first");
    CHECK_INT_EQ(ingest_dequeue(&out), 1);
    CHECK_STR_EQ(out.source, "second");
    CHECK_INT_EQ(ingest_queue_depth(), 0);
}

static void test_queue_drops_oldest_when_full(void) {
    char name[32];
    for (int i = 0; i < INGEST_QUEUE_SIZE + 2; i++) {
        snprintf(name, sizeof(name), "src%d", i);
        ingest_record_t r = make_rec(name);
        queue_push(&r);
    }
    /* Depth is capped and the two oldest records were evicted. */
    CHECK_INT_EQ(ingest_queue_depth(), INGEST_QUEUE_SIZE);

    ingest_record_t out;
    CHECK_INT_EQ(ingest_dequeue(&out), 1);
    CHECK_STR_EQ(out.source, "src2");

    while (ingest_queue_depth() > 0) ingest_dequeue(&out);
}

static void test_dequeue_returns_zero_when_stopped_and_empty(void) {
    /* g_running is 0 until ingest_start(), so an empty dequeue must not block. */
    ingest_record_t out;
    CHECK_INT_EQ(ingest_queue_depth(), 0);
    CHECK_INT_EQ(ingest_dequeue(&out), 0);
}

int main(void) {
    gt_env_init();
    RUN_TEST(test_parse_full_record);
    RUN_TEST(test_parse_context_keeps_pipes);
    RUN_TEST(test_parse_optional_fields);
    RUN_TEST(test_parse_rejects_malformed);
    RUN_TEST(test_parse_empty_fields_shift_left);
    RUN_TEST(test_parse_query_records);
    RUN_TEST(test_parse_truncates_oversized_fields);
    RUN_TEST(test_queue_fifo_order);
    RUN_TEST(test_queue_drops_oldest_when_full);
    RUN_TEST(test_dequeue_returns_zero_when_stopped_and_empty);
    return test_report();
}
