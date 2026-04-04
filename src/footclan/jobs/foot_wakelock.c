// foot_wakelock.c — Footrunner wakelock probe via sysport dumpsys power

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *foot_dumpsys(const char *service);
void foot_emit_result(const char *fmt, ...);

static int count_wakelocks(const char *s) {
    // Very rough: count occurrences of "Wake Locks: size="
    int count = 0;
    const char *p = s;
    while ((p = strstr(p, "Wake Locks:")) != NULL) {
        count++;
        p += 10;
    }
    return count;
}

void foot_job_wakelock(const char *job_id) {
    char *dump = foot_dumpsys("power");
    if (!dump || strncmp(dump, "OK ", 3) != 0) {
        if (dump) free(dump);
        foot_emit_result("FOOT RESULT %s ERR=WAKELOCK_UNAVAILABLE", job_id);
        return;
    }

    int wl = count_wakelocks(dump + 3);
    free(dump);

    foot_emit_result("FOOT RESULT %s WAKELOCK_SETS=%d", job_id, wl);
}
