#ifndef MP_TEST_HARNESS_H
#define MP_TEST_HARNESS_H

/*
 * test_harness.h — minimal assertion harness for the MiuiserPeruser unit tests.
 *
 * No external dependencies: each test binary is a plain C program that runs a
 * list of test functions and exits non-zero if any assertion failed.
 *
 *   static void test_thing(void) { CHECK(1 + 1 == 2); }
 *   int main(void) { RUN_TEST(test_thing); return test_report(); }
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

static int t_failures;
static int t_checks;
static int t_tests;
static const char *t_current = "<none>";

static void t_fail(const char *file, int line, const char *msg) {
    t_failures++;
    fprintf(stderr, "  FAIL %s (%s:%d): %s\n", t_current, file, line, msg);
}

#define CHECK(cond)                                                      \
    do {                                                                 \
        t_checks++;                                                      \
        if (!(cond)) t_fail(__FILE__, __LINE__, "CHECK(" #cond ")");     \
    } while (0)

#define CHECK_MSG(cond, ...)                                             \
    do {                                                                 \
        t_checks++;                                                      \
        if (!(cond)) {                                                   \
            char _m[512];                                                \
            snprintf(_m, sizeof(_m), __VA_ARGS__);                       \
            t_fail(__FILE__, __LINE__, _m);                              \
        }                                                                \
    } while (0)

#define CHECK_INT_EQ(actual, expected)                                   \
    do {                                                                 \
        long _a = (long)(actual), _e = (long)(expected);                 \
        t_checks++;                                                      \
        if (_a != _e) {                                                  \
            char _m[256];                                                \
            snprintf(_m, sizeof(_m), #actual " == " #expected            \
                     " (got %ld, want %ld)", _a, _e);                    \
            t_fail(__FILE__, __LINE__, _m);                              \
        }                                                                \
    } while (0)

#define CHECK_DBL_EQ(actual, expected)                                   \
    do {                                                                 \
        double _a = (double)(actual), _e = (double)(expected);           \
        t_checks++;                                                      \
        if (fabs(_a - _e) > 1e-6) {                                      \
            char _m[256];                                                \
            snprintf(_m, sizeof(_m), #actual " == " #expected            \
                     " (got %.6f, want %.6f)", _a, _e);                  \
            t_fail(__FILE__, __LINE__, _m);                              \
        }                                                                \
    } while (0)

#define CHECK_STR_EQ(actual, expected)                                   \
    do {                                                                 \
        const char *_a = (actual), *_e = (expected);                     \
        t_checks++;                                                      \
        if (!_a || !_e || strcmp(_a, _e) != 0) {                         \
            char _m[512];                                                \
            snprintf(_m, sizeof(_m), #actual " == " #expected            \
                     " (got \"%s\", want \"%s\")",                       \
                     _a ? _a : "(null)", _e ? _e : "(null)");            \
            t_fail(__FILE__, __LINE__, _m);                              \
        }                                                                \
    } while (0)

#define RUN_TEST(fn)                                                     \
    do {                                                                 \
        t_current = #fn;                                                 \
        t_tests++;                                                       \
        int _before = t_failures;                                        \
        fn();                                                            \
        printf("  %-4s %s\n", (t_failures == _before) ? "ok" : "FAIL",   \
               #fn);                                                     \
    } while (0)

static int test_report(void) {
    printf("%s: %d tests, %d checks, %d failures\n",
           t_failures ? "FAILED" : "PASSED", t_tests, t_checks, t_failures);
    return t_failures ? 1 : 0;
}

#endif /* MP_TEST_HARNESS_H */
