#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "log.h"

// ------------------------------------------------------------
// File logging setup
// ------------------------------------------------------------

static FILE *log_file = NULL;

static const char *log_dir =
    "/data/data/com.termux/files/home/.local/logs/miuiserperuser";

static const char *log_file_path =
    "/data/data/com.termux/files/home/.local/logs/miuiserperuser/current.log";

static void ensure_log_dir(void) {
    mkdir("/data/data/com.termux/files/home/.local", 0755);
    mkdir("/data/data/com.termux/files/home/.local/logs", 0755);
    mkdir(log_dir, 0755);
}

static void open_log_file(void) {
    if (log_file)
        return;

    ensure_log_dir();
    log_file = fopen(log_file_path, "a");
}

// ------------------------------------------------------------
// Logging core
// ------------------------------------------------------------

static const char *log_prefix = "daemon";
static log_level_t current_level = LOG_DEBUG;

static const char *level_color[] = {
    "\033[34m", // DEBUG - blue
    "\033[32m", // INFO  - green
    "\033[33m", // WARN  - yellow
    "\033[31m"  // ERROR - red
};

static const char *level_name[] = {
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR"
};

void log_set_prefix(const char *name) {
    log_prefix = name;
}

void log_set_level(log_level_t level) {
    current_level = level;
}

static void log_emit(log_level_t level, const char *fmt, va_list args) {
    if (level < current_level)
        return;

    // --- Timestamp ---
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm_info);

    // --- Terminal output (colored) ---
    fprintf(stderr, "%s[%s][%s][%s] ",
            level_color[level],
            ts,
            log_prefix,
            level_name[level]);

    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\033[0m\n");

    // --- File output ---
    open_log_file();
    if (log_file) {
        fprintf(log_file, "[%s][%s][%s] ",
                ts,
                log_prefix,
                level_name[level]);

        vfprintf(log_file, fmt, args);
        fprintf(log_file, "\n");
        fflush(log_file);
    }
}

// ------------------------------------------------------------
// Public logging API
// ------------------------------------------------------------

void log_debug(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_emit(LOG_DEBUG, fmt, args);
    va_end(args);
}

void log_info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_emit(LOG_INFO, fmt, args);
    va_end(args);
}

void log_warn(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_emit(LOG_WARN, fmt, args);
    va_end(args);
}

void log_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_emit(LOG_ERROR, fmt, args);
    va_end(args);
}
