// foot_battery.c — Footrunner battery probe via sysport dumpsys + sysfs

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *foot_dumpsys(const char *service);   // from foot_sys.c
char *foot_read_sys(const char *path);
void foot_emit_result(const char *fmt, ...);

static int parse_dumpsys_battery_level(const char *s) {
    // Look for "level: N"
    const char *p = strstr(s, "level:");
    if (!p) return -1;
    p += 6;
    while (*p == ' ' || *p == '\t') p++;
    return atoi(p);
}

static int read_sys_batt_level(void) {
    char *raw = foot_read_sys("/sys/class/power_supply/battery/capacity");
    if (!raw) return -1;
    if (strncmp(raw, "OK ", 3) != 0) {
        free(raw);
        return -1;
    }
    int lvl = atoi(raw + 3);
    free(raw);
    return lvl;
}

void foot_job_battery(const char *job_id) {
    int level = -1;

    char *dump = foot_dumpsys("battery");
    if (dump && strncmp(dump, "OK ", 3) == 0) {
        level = parse_dumpsys_battery_level(dump + 3);
    }
    if (dump) free(dump);

    if (level < 0) {
        level = read_sys_batt_level();
    }

    if (level < 0) {
        foot_emit_result("FOOT RESULT %s ERR=BATT_UNAVAILABLE", job_id);
        return;
    }

    foot_emit_result("FOOT RESULT %s BATT_LEVEL=%d", job_id, level);
}
