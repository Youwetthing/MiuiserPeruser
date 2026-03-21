#include "log_safe.h"
#include "fugitoid_log.h"   // reuse your structured backend

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

static char g_component[64] = "core";
static log_level_t g_level = LOG_LEVEL_INFO;

static const char *level_str(log_level_t lvl) {
    switch (lvl) {
        case LOG_LEVEL_DEBUG: return "DEBUG";
        case LOG_LEVEL_INFO:  return "INFO";
        case LOG_LEVEL_WARN:  return "WARN";
        case LOG_LEVEL_ERROR: return "ERROR";
        default:              return "INFO";
    }
}

void log_init(const char *component) {
    if (component && *component) {
        snprintf(g_component, sizeof(g_component), "%s", component);
    }
}

void log_set_level(log_level_t level) {
    g_level = level;
}

void log_event(log_level_t level, const char *tag, const char *fmt, ...) {
    if (level < g_level) return;

    char msg[512];

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    // IMPORTANT: msg must already be sanitised by caller:
    // no raw dumps, no PII, no full command outputs.

    fugitoid_log_json(
        level_str(level),          // level
        "miuiserperuser",          // domain
        g_component,               // component (daemon name)
        tag ? tag : "EVENT",       // event
        NULL,                      // correlation_id (optional)
        msg,                       // message
        "{}"                       // meta_json (extend later if needed)
    );
}
