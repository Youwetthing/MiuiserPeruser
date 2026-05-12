// footrunner.c — TMNT-era Foot Clan Runner
// Unified Foot Clan job executor: probes, scans, diagnostics.
// Replaces all legacy Foot daemons with a single-shot job engine.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUF_SIZE 2048

// -------------------------------
// Utility: run a shell command
// -------------------------------
static char *run_cmd(const char *cmd) {
    FILE *f = popen(cmd, "r");
    if (!f) return NULL;

    char *buf = malloc(BUF_SIZE);
    if (!buf) {
        pclose(f);
        return NULL;
    }

    if (!fgets(buf, BUF_SIZE, f)) {
        free(buf);
        pclose(f);
        return NULL;
    }

    buf[strcspn(buf, "\n")] = '\0';
    pclose(f);
    return buf;
}

// -------------------------------
// Utility: print Foot result
// -------------------------------
static void print_result(const char *job_id, const char *kv) {
    if (!job_id) job_id = "0";
    if (!kv) kv = "OK=0 ERROR=unknown";
    printf("FOOT RESULT %s %s\n", job_id, kv);
}

// ============================================================
//  REPURPOSED LEGACY FOOT CLAN JOBS
// ============================================================

// 1. Legacy foot_ipcshadowd → IPC_SHADOW_CHECK
static void job_ipc_shadow_check(const char *job_id) {
    // Check if Turtlecom socket exists
    if (access("/data/data/com.termux/files/home/.turtlecom.sock", F_OK) == 0) {
        print_result(job_id, "OK=1 TURTLECOM_SOCKET=present");
    } else {
        print_result(job_id, "OK=0 TURTLECOM_SOCKET=missing");
    }
}

// 2. Legacy foot_heartbeatd → HEARTBEAT_PULSE
static void job_heartbeat_pulse(const char *job_id) {
    print_result(job_id, "OK=1 PULSE=alive");
}

// 3. Legacy foot_tempbackupd → THERMAL_FALLBACK
static void job_thermal_fallback(const char *job_id) {
    char *r = run_cmd("cat /sys/class/thermal/thermal_zone0/temp 2>/dev/null");
    if (!r) {
        print_result(job_id, "OK=0 TEMP=unavailable");
        return;
    }
    char kv[BUF_SIZE];
    snprintf(kv, sizeof(kv), "OK=1 TEMP=%s", r);
    free(r);
    print_result(job_id, kv);
}

// 4. Legacy foot_zombiebackupd → PROCESS_FALLBACK
static void job_process_fallback(const char *job_id) {
    char *r = run_cmd("ps -A | wc -l");
    if (!r) {
        print_result(job_id, "OK=0 PROCS=unavailable");
        return;
    }
    char kv[BUF_SIZE];
    snprintf(kv, sizeof(kv), "OK=1 PROCS=%s", r);
    free(r);
    print_result(job_id, kv);
}

// 5. Legacy foot_ipcwatchd → IPC_WATCH
static void job_ipc_watch(const char *job_id) {
    // Check if Turtlecom is responsive by checking socket existence
    if (access("/data/data/com.termux/files/home/.turtlecom.sock", F_OK) == 0) {
        print_result(job_id, "OK=1 TURTLECOM_HEALTH=good");
    } else {
        print_result(job_id, "OK=0 TURTLECOM_HEALTH=dead");
    }
}

// ============================================================
//  CORE FOOT JOBS (NEW TMNT-ERA)
// ============================================================

// PROBE_DUMPSYS
static void job_probe_dumpsys(const char *job_id) {
    char *r = run_cmd("dumpsys -l 2>/dev/null | head -n 1");
    if (!r) {
        print_result(job_id, "OK=0 DUMPSYS_ALLOWED=0");
        return;
    }
    free(r);
    print_result(job_id, "OK=1 DUMPSYS_ALLOWED=1");
}

// QUICK_CPU
static void job_quick_cpu(const char *job_id) {
    char *line = run_cmd("top -b -n 1 2>/dev/null | sed -n '8p'");
    if (!line) {
        print_result(job_id, "OK=0 CPU_TOP=unavailable");
        return;
    }
    char kv[BUF_SIZE];
    snprintf(kv, sizeof(kv), "OK=1 CPU_TOP=\"%s\"", line);
    free(line);
    print_result(job_id, kv);
}

// PROBE_THERMAL
static void job_probe_thermal(const char *job_id) {
    char *r = run_cmd("ls /sys/class/thermal 2>/dev/null | head -n 1");
    if (!r) {
        print_result(job_id, "OK=0 THERMAL_SYS=missing");
        return;
    }
    free(r);
    print_result(job_id, "OK=1 THERMAL_SYS=present");
}

// PROBE_MIUI
static void job_probe_miui(const char *job_id) {
    char *r = run_cmd("getprop ro.miui.ui.version.name 2>/dev/null");
    if (!r || strlen(r) == 0) {
        print_result(job_id, "OK=0 MIUI=absent");
        if (r) free(r);
        return;
    }
    char kv[BUF_SIZE];
    snprintf(kv, sizeof(kv), "OK=1 MIUI_VERSION=%s", r);
    free(r);
    print_result(job_id, kv);
}

// PROBE_WAKELOCKS
static void job_probe_wakelocks(const char *job_id) {
    char *r = run_cmd("cat /sys/power/wake_lock 2>/dev/null | head -n 1");
    if (!r) {
        print_result(job_id, "OK=0 WAKELOCKS=unavailable");
        return;
    }
    free(r);
    print_result(job_id, "OK=1 WAKELOCKS=readable");
}

// PROBE_SECURITY
static void job_probe_security(const char *job_id) {
    char *r = run_cmd("getenforce 2>/dev/null");
    if (!r) {
        print_result(job_id, "OK=0 SELINUX=unknown");
        return;
    }
    char kv[BUF_SIZE];
    snprintf(kv, sizeof(kv), "OK=1 SELINUX=%s", r);
    free(r);
    print_result(job_id, kv);
}

// PROBE_INTEGRITY
static void job_probe_integrity(const char *job_id) {
    char *r = run_cmd("cat /proc/sys/kernel/tainted 2>/dev/null");
    if (!r) {
        print_result(job_id, "OK=0 INTEGRITY=unknown");
        return;
    }
    char kv[BUF_SIZE];
    snprintf(kv, sizeof(kv), "OK=1 TAINT=%s", r);
    free(r);
    print_result(job_id, kv);
}

// ============================================================
//  FULL SCAN JOBS
// ============================================================

// SCAN_WAKELOCKS_FULL
static void job_scan_wakelocks_full(const char *job_id) {
    char *r = run_cmd("cat /sys/power/wake_lock 2>/dev/null");
    if (!r) {
        print_result(job_id, "OK=0 WAKELOCKS_FULL=unavailable");
        return;
    }
    char kv[BUF_SIZE];
    snprintf(kv, sizeof(kv), "OK=1 WAKELOCKS_FULL=\"%s\"", r);
    free(r);
    print_result(job_id, kv);
}

// SCAN_PROCESSES_FULL
static void job_scan_processes_full(const char *job_id) {
    char *r = run_cmd("ps -A 2>/dev/null");
    if (!r) {
        print_result(job_id, "OK=0 PROCS_FULL=unavailable");
        return;
    }
    char kv[BUF_SIZE];
    snprintf(kv, sizeof(kv), "OK=1 PROCS_FULL=\"%s\"", r);
    free(r);
    print_result(job_id, kv);
}

// ============================================================
//  UNKNOWN JOB HANDLER
// ============================================================
static void job_unknown(const char *job_id, const char *type) {
    char kv[BUF_SIZE];
    snprintf(kv, sizeof(kv), "OK=0 UNKNOWN_JOB=\"%s\"", type ? type : "null");
    print_result(job_id, kv);
}

// ============================================================
//  MAIN DISPATCHER
// ============================================================
int main(int argc, char **argv) {
    if (argc < 3) {
        print_result("0", "OK=0 ERROR=bad_args");
        return 1;
    }

    const char *job_id = argv[1];
    const char *job_type = argv[2];

    // Repurposed legacy jobs
    if (strcmp(job_type, "IPC_SHADOW_CHECK") == 0) job_ipc_shadow_check(job_id);
    else if (strcmp(job_type, "HEARTBEAT_PULSE") == 0) job_heartbeat_pulse(job_id);
    else if (strcmp(job_type, "THERMAL_FALLBACK") == 0) job_thermal_fallback(job_id);
    else if (strcmp(job_type, "PROCESS_FALLBACK") == 0) job_process_fallback(job_id);
    else if (strcmp(job_type, "IPC_WATCH") == 0) job_ipc_watch(job_id);

    // Core jobs
    else if (strcmp(job_type, "PROBE_DUMPSYS") == 0) job_probe_dumpsys(job_id);
    else if (strcmp(job_type, "QUICK_CPU") == 0) job_quick_cpu(job_id);
    else if (strcmp(job_type, "PROBE_THERMAL") == 0) job_probe_thermal(job_id);
    else if (strcmp(job_type, "PROBE_MIUI") == 0) job_probe_miui(job_id);
    else if (strcmp(job_type, "PROBE_WAKELOCKS") == 0) job_probe_wakelocks(job_id);
    else if (strcmp(job_type, "PROBE_SECURITY") == 0) job_probe_security(job_id);
    else if (strcmp(job_type, "PROBE_INTEGRITY") == 0) job_probe_integrity(job_id);

    // Full scans
    else if (strcmp(job_type, "SCAN_WAKELOCKS_FULL") == 0) job_scan_wakelocks_full(job_id);
    else if (strcmp(job_type, "SCAN_PROCESSES_FULL") == 0) job_scan_processes_full(job_id);

    else job_unknown(job_id, job_type);

    return 0;
}
