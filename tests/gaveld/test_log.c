/* Unit tests for src/gaveld/log.c — log file handling and rotation. */

#include "test_harness.h"
#include "gaveld_test_env.h"
#include "log.h"

#include <stdlib.h>
#include <sys/stat.h>

#define ROTATED_PATH LOG_PATH ".1"

static long file_size(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0) ? (long)st.st_size : -1;
}

static int file_contains(const char *path, const char *needle) {
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;
    char line[2048];
    int found = 0;
    while (!found && fgets(line, sizeof(line), fp))
        if (strstr(line, needle)) found = 1;
    fclose(fp);
    return found;
}

static void test_glog_before_open_is_dropped(void) {
    /* No log file yet: glog must not crash and must not create one. */
    glog("INFO", "message before log_open");
    CHECK_INT_EQ(file_size(LOG_PATH), -1);
}

static void test_open_and_write(void) {
    CHECK_INT_EQ(log_open(), 0);
    glog("WARN", "hello %s %d", "world", 42);
    log_flush();
    CHECK(file_contains(LOG_PATH, "[GAVELD] "));
    CHECK(file_contains(LOG_PATH, "[WARN] hello world 42"));
}

static void test_append_across_reopen(void) {
    log_close();
    CHECK_INT_EQ(log_open(), 0);
    glog("INFO", "second session");
    log_flush();
    /* Reopening appends — the earlier line survives. */
    CHECK(file_contains(LOG_PATH, "hello world 42"));
    CHECK(file_contains(LOG_PATH, "second session"));
}

static void test_long_message_is_truncated_not_overflowed(void) {
    char big[4096];
    memset(big, 'x', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';
    glog("INFO", "%s", big);
    log_flush();
    /* Formatted message is capped at 1024 bytes inside glog(). */
    CHECK(file_contains(LOG_PATH, "xxxxxxxx"));
}

static void test_rotation_at_size_limit(void) {
    unlink(ROTATED_PATH);

    char filler[512];
    memset(filler, 'a', sizeof(filler) - 1);
    filler[sizeof(filler) - 1] = '\0';

    while (file_size(LOG_PATH) < LOG_MAX_BYTES)
        glog("INFO", "%s", filler);
    log_flush();

    /* The next write notices the size and rotates before logging. */
    glog("INFO", "post rotation marker");
    log_flush();

    CHECK(file_size(ROTATED_PATH) >= LOG_MAX_BYTES);
    CHECK(file_size(LOG_PATH) < LOG_MAX_BYTES);
    CHECK(file_contains(LOG_PATH, "post rotation marker"));
    CHECK_INT_EQ(file_contains(ROTATED_PATH, "post rotation marker"), 0);

    unlink(ROTATED_PATH);
}

static void test_close_is_idempotent(void) {
    log_close();
    log_close();
    glog("INFO", "dropped after close");
    CHECK_INT_EQ(file_contains(LOG_PATH, "dropped after close"), 0);
}

int main(void) {
    gt_env_init();
    unlink(LOG_PATH);
    unlink(ROTATED_PATH);

    RUN_TEST(test_glog_before_open_is_dropped);
    RUN_TEST(test_open_and_write);
    RUN_TEST(test_append_across_reopen);
    RUN_TEST(test_long_message_is_truncated_not_overflowed);
    RUN_TEST(test_rotation_at_size_limit);
    RUN_TEST(test_close_is_idempotent);

    unlink(LOG_PATH);
    return test_report();
}
