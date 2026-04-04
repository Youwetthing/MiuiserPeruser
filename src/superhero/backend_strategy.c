#include "backend_strategy.h"
#include "april_event.h"

extern void april_log(const char* level, const char* format, ...);

BACKEND_TYPE backend_strategy_select(void) {
    april_log("INFO", "Backend Strategy: selecting backend…");

    int rc = backend_select_best();
    const backend_info_t *info = backend_get_active_info();

    BACKEND_TYPE t = BACKEND_NONE;
    if (info)
        t = info->type;

    april_log("INFO", "Backend Strategy: rc=%d, type=%s",
              rc, backend_name(t));

    return t;
}

const char *backend_name(BACKEND_TYPE t) {
    switch (t) {
        case BACKEND_APRIL:           return "APRIL";
        case BACKEND_RISH:            return "RISH";
        case BACKEND_SHIZUKU:         return "SHIZUKU";
        case BACKEND_ADB:             return "ADB";
        case BACKEND_SYSPORT:         return "SYSPORT";
        case BACKEND_TERMUX_FALLBACK: return "TERMUX_FALLBACK";
        case BACKEND_NONE:
        default:
            return "NONE";
    }
}
