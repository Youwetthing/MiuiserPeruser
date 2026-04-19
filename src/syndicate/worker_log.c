#include "fugitoid_log.h"
#include "fugitoid_log.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

void fugitoid_log(const char *level, const char *format, ...) {
    FILE *f = fopen("/data/data/com.termux/files/home/miuiserperuser.log", "a");
    if (!f) return;

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    char tbuf[32];
    strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", tm_info);

    fprintf(f, "[%s] [%s] ", tbuf, level);

    va_list args;
    va_start(args, format);
    vfprintf(f, format, args);
    va_end(args);

    fprintf(f, "\n");
    fclose(f);
}
