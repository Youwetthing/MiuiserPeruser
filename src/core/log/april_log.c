#include <stdio.h>
#include <stdarg.h>
#include <time.h>

void april_log(const char* level, const char* format, ...) {
    va_list args;
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);

    // Print to terminal
    fprintf(stdout, "[%02d:%02d:%02d] [%s] ",
            tm->tm_hour, tm->tm_min, tm->tm_sec, level);
    va_start(args, format);
    vfprintf(stdout, format, args);
    fprintf(stdout, "\n");
    fflush(stdout);

    // Also write to log file
    va_end(args);
    FILE *log = fopen("/data/data/com.termux/files/home/miuiserperuser.log", "a");
    if (log) {
        fprintf(log, "[%04d-%02d-%02d %02d:%02d:%02d] [%s] ",
                tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
                tm->tm_hour, tm->tm_min, tm->tm_sec, level);
        va_start(args, format);
        vfprintf(log, format, args);
        fprintf(log, "\n");
        fclose(log);
    }
    va_end(args);
}
