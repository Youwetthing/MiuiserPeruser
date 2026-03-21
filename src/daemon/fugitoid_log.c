#include "fugitoid_log.h"
#include "fugitoid_log.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>

#include "fugitoid_log.h"

static char log_prefix[64] = {0};

void fugitoid_init(const char *name) {
    snprintf(log_prefix, sizeof(log_prefix), "[%s][FUGITOID]", name);
}

void fugitoid_log(const char *level, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    char timebuf[32];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm_info);

    printf("%s %s %s ", timebuf, log_prefix, level);
    vprintf(fmt, args);
    printf("\n");

    va_end(args);
}
