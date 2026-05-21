#ifndef MIUISERPERUSER_LOG_H
#define MIUISERPERUSER_LOG_H

#include <stdarg.h>

typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
} log_level_t;

void log_init(const char *component);
void log_set_level(log_level_t level);
void log_event(log_level_t level, const char *tag, const char *fmt, ...);
void log_debug(const char *fmt, ...);
void log_info (const char *fmt, ...);
void log_warn (const char *fmt, ...);
void log_error(const char *fmt, ...);

#endif
