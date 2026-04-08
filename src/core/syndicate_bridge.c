#include "fugitoid_log.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <sys/stat.h>

void fugitoid_init() {
    // Ensures the workspace is ready for the daemons
    mkdir("/data/data/com.termux/files/home/MiuiserPeruser/pipes", 0777);
    mkdir("/data/data/com.termux/files/home/MiuiserPeruser/logs", 0777);
}

void fugitoid_log(const char* level, const char* fmt, ...) {
    va_list args;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    
    // Clean terminal output for monitoring
    printf("[%02d:%02d:%02d] [%s] ", t->tm_hour, t->tm_min, t->tm_sec, level);
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}