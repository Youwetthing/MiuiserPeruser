#include <stdio.h>
#include <stdarg.h>
#include <time.h>

void april_log(const char* level, const char* format, ...) {
    FILE *log = fopen("/data/data/com.termux/files/home/miuiserperuser.log", "a");
    if (!log) return;
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    fprintf(log, "[%04d-%02d-%02d %02d:%02d:%02d] [%s] ",
            tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
            tm->tm_hour, tm->tm_min, tm->tm_sec, level);
    va_list args;
    va_start(args, format);
    vfprintf(log, format, args);
    va_end(args);
    fprintf(log, "\n");
    fclose(log);
}
