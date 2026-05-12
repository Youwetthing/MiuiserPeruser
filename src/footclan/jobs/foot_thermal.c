// foot_thermal.c — Footrunner thermal probe using sysport backend

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *foot_read_sys(const char *path);   // from foot_sys.c
void foot_emit_result(const char *fmt, ...);

// ------------------------------------------------------------
// Probe a single thermal zone
// ------------------------------------------------------------
static int probe_zone(const char *zone_path) {
    char *raw = foot_read_sys(zone_path);
    if (!raw) return -1;

    // Expect: "OK <value>" or "ERR <reason>"
    if (strncmp(raw, "OK ", 3) != 0) {
        free(raw);
        return -1;
    }

    char *val = raw + 3;
    int temp = atoi(val);
    free(raw);

    return temp;
}

// ------------------------------------------------------------
// Main thermal job
// ------------------------------------------------------------
void foot_job_thermal(const char *job_id) {
    int cpu0 = probe_zone("/sys/class/thermal/thermal_zone0/temp");
    int cpu1 = probe_zone("/sys/class/thermal/thermal_zone1/temp");
    int batt = probe_zone("/sys/class/thermal/thermal_zone2/temp");

    foot_emit_result(
        "FOOT RESULT %s CPU0=%d CPU1=%d BATT=%d",
        job_id,
        cpu0,
        cpu1,
        batt
    );
}
