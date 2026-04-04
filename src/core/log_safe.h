#ifndef MIUISERPERUSER_LOG_SAFE_H
#define MIUISERPERUSER_LOG_SAFE_H

#include <stdarg.h>

typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} log_level_t;

void log_init(const char *component);          // e.g. "daemon", "thermald"
void log_set_level(log_level_t level);         // default: INFO
void log_event(log_level_t level,
               const char *tag,
               const char *fmt, ...);          // main entry point

#endif
