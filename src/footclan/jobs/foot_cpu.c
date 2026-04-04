// foot_cpu.c — Footrunner CPU frequency probe via sysport /sys

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *foot_read_sys(const char *path);
void foot_emit_result(const char *fmt, ...);

static int read_cpu_freq(const char *path) {
    char *raw = foot_read_sys(path);
    if (!raw) return -1;
    if (strncmp(raw, "OK ", 3) != 0) {
        free(raw);
        return -1;
    }
    int khz = atoi(raw + 3);
    free(raw);
    return khz;
}

void foot_job_cpu(const char *job_id) {
    int cpu0 = read_cpu_freq("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
    int cpu1 = read_cpu_freq("/sys/devices/system/cpu/cpu1/cpufreq/scaling_cur_freq");

    foot_emit_result("FOOT RESULT %s CPU0_KHZ=%d CPU1_KHZ=%d", job_id, cpu0, cpu1);
}
