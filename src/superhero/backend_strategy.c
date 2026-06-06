#include "backend_strategy.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

extern void april_log(const char* level, const char* format, ...);

static int rish_alive(void) {
    FILE *fp = popen(
        "RISH_APPLICATION_ID=com.termux "
        "/data/data/com.termux/files/home/Rish/rish -c 'echo alive' 2>/dev/null",
        "r");
    if (!fp) return 0;
    char buf[16] = {0};
    fgets(buf, sizeof(buf), fp);
    pclose(fp);
    return strncmp(buf, "alive", 5) == 0;
}

static int adb_alive(void) {
    FILE *fp = popen("adb -s 127.0.0.1:5555 shell echo alive 2>/dev/null", "r");
    if (!fp) return 0;
    char buf[16] = {0};
    fgets(buf, sizeof(buf), fp);
    pclose(fp);
    return strncmp(buf, "alive", 5) == 0;
}

BACKEND_TYPE backend_strategy_select(void) {
    if (rish_alive()) {
        april_log("INFO", "BACKEND: rish available — using elevated shell");
        return BACKEND_RISH;
    }
    april_log("WARN", "BACKEND: rish unavailable — trying ADB TCP");
    if (adb_alive()) {
        april_log("INFO", "BACKEND: ADB TCP available — using fallback");
        return BACKEND_ADB;
    }
    april_log("WARN", "BACKEND: no privileged backend — limited scan only");
    return BACKEND_LOCAL;
}

const char *backend_name(BACKEND_TYPE b) {
    switch (b) {
        case BACKEND_RISH:  return "rish";
        case BACKEND_ADB:   return "adb";
        case BACKEND_LOCAL: return "local";
        default:            return "none";
    }
}
