// foot_process.c — Footrunner process/load probe via sysport /proc

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *foot_read_proc(const char *path);   // from foot_sys.c
void foot_emit_result(const char *fmt, ...);

// Parse first line of /proc/stat: "cpu  user nice system idle ..."
static int parse_cpu_load(const char *line) {
    // Very rough: sum of all fields except idle
    unsigned long user, nice, system, idle, iowait, irq, softirq, steal;
    int n = sscanf(line, "cpu %lu %lu %lu %lu %lu %lu %lu %lu",
                   &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);
    if (n < 4) return -1;
    unsigned long busy = user + nice + system + iowait + irq + softirq + steal;
    unsigned long total = busy + idle;
    if (total == 0) return -1;
    int pct = (int)((busy * 100) / total);
    return pct;
}

void foot_job_process(const char *job_id) {
    char *raw = foot_read_proc("/proc/stat");
    if (!raw) {
        foot_emit_result("FOOT RESULT %s ERR=PROC_READ_FAILED", job_id);
        return;
    }

    if (strncmp(raw, "OK ", 3) != 0) {
        foot_emit_result("FOOT RESULT %s ERR=PROC_BACKEND", job_id);
        free(raw);
        return;
    }

    char *body = raw + 3;
    // First token up to space is "cpu ..."
    char *line = strtok(body, "\n");
    if (!line) line = body;

    int load = parse_cpu_load(line);
    free(raw);

    foot_emit_result("FOOT RESULT %s CPU_LOAD=%d", job_id, load);
}
