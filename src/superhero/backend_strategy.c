#include "backend_strategy.h"
#include "april_event.h"

extern void april_log(const char* level, const char* format, ...);

BACKEND_TYPE backend_strategy_select(void) {
    april_log("INFO", "Backend Strategy: forcing RISH backend");
    return BACKEND_RISH;
}

const char *backend_name(BACKEND_TYPE t) {
    switch (t) {
        case BACKEND_RISH:            return "RISH";
        case BACKEND_SHIZUKU:         return "SHIZUKU";
        case BACKEND_ADB:             return "ADB";
        case BACKEND_NONE:
        default:                      return "NONE";
    }
}
