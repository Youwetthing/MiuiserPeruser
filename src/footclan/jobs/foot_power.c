// foot_power.c — Footrunner power state via sysport dumpsys power

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *foot_dumpsys(const char *service);
void foot_emit_result(const char *fmt, ...);

static int parse_screen_on(const char *s) {
    // Look for "mHoldingDisplaySuspendBlocker=true/false" or "Display Power: state=ON/OFF"
    if (strstr(s, "state=ON") || strstr(s, "mHoldingDisplaySuspendBlocker=true"))
        return 1;
    if (strstr(s, "state=OFF") || strstr(s, "mHoldingDisplaySuspendBlocker=false"))
        return 0;
    return -1;
}

void foot_job_power(const char *job_id) {
    char *dump = foot_dumpsys("power");
    if (!dump || strncmp(dump, "OK ", 3) != 0) {
        if (dump) free(dump);
        foot_emit_result("FOOT RESULT %s ERR=POWER_UNAVAILABLE", job_id);
        return;
    }

    int screen_on = parse_screen_on(dump + 3);
    free(dump);

    foot_emit_result("FOOT RESULT %s SCREEN_ON=%d", job_id, screen_on);
}
