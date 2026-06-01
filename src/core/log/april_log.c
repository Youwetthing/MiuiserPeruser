#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include "../../src/gaveld/gaveld_emit.h"

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

    /* Wire THREAT and WARN level events to gaveld judicial pipeline */
    if (strcmp(level, "THREAT") == 0 || strcmp(level, "WARN") == 0) {
        char signal[64] = "SUPERHERO_ANOMALY";
        char ctx[256];
        va_list args2;
        va_start(args2, format);
        vsnprintf(ctx, sizeof(ctx), format, args2);
        va_end(args2);
        /* Map level to signal */
        if (strcmp(level, "THREAT") == 0) {
            /* Extract signal name from message prefix e.g. "KERNEL: ..." */
            if (strncmp(ctx, "KERNEL:", 7) == 0)
                strncpy(signal, "SUPERHERO_KERNEL_THREAT", sizeof(signal)-1);
            else if (strncmp(ctx, "BEHAVIOR:", 9) == 0)
                strncpy(signal, "SUPERHERO_BEHAVIOR_THREAT", sizeof(signal)-1);
            else if (strncmp(ctx, "INTEGRITY:", 10) == 0)
                strncpy(signal, "SUPERHERO_INTEGRITY_THREAT", sizeof(signal)-1);
            else if (strncmp(ctx, "MEMORY:", 7) == 0 ||
                     strncmp(ctx, "Woah", 4) == 0)
                strncpy(signal, "SUPERHERO_MEMORY_THREAT", sizeof(signal)-1);
            else if (strncmp(ctx, "Persistence:", 12) == 0)
                strncpy(signal, "SUPERHERO_PERSISTENCE_THREAT", sizeof(signal)-1);
        }
        gaveld_emit("superhero", signal, 0.0, ctx);
    }

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
