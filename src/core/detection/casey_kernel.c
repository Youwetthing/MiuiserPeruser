#include "compat/sensei_compat.h"
#include "sensei_types.h"
#include "rish_pipe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern void april_log(const char* level, const char* format, ...);
extern SENSEI_STATUS april_detection_list_append(SENSEI_DETECTION_LIST *list, const SENSEI_DETECTION *det);

#define KMOD_BASELINE "/data/data/com.termux/files/home/MiuiserPeruser/data/kmod_baseline.txt"

static char *rish_cmd(const char *cmd) {
    char full[2048];
    snprintf(full, sizeof(full),
        "RISH_APPLICATION_ID=com.termux "
        "/data/data/com.termux/files/home/Rish/rish -c '%s' 2>/dev/null", cmd);
    FILE *fp = popen(full, "r");
    if (!fp) return NULL;
    size_t sz = 8192; char *out = malloc(sz);
    if (!out) { pclose(fp); return NULL; }
    out[0] = 0; size_t pos = 0; char buf[512];
    while (fgets(buf, sizeof(buf), fp)) {
        size_t len = strlen(buf);
        if (pos + len + 1 >= sz) { sz *= 2; char *t = realloc(out, sz); if (!t) break; out = t; }
        memcpy(out + pos, buf, len); pos += len;
    }
    out[pos] = 0; pclose(fp); return out;
}

static void add_det(SENSEI_DETECTION_LIST *r,
                    SENSEI_DETECTION_CLASS cls, SENSEI_EVENT_PRIORITY pri,
                    SENSEI_MITRE_TECHNIQUE mitre, int conf,
                    const char *type, const char *desc) {
    SENSEI_DETECTION det = {0};
    det.detection_class = cls; det.priority = pri;
    det.mitre_id = mitre; det.confidence = conf;
    strncpy(det.detection_type, type, SENSEI_MAX_DETECTION_TYPE - 1);
    strncpy(det.description,    desc, SENSEI_MAX_DESCRIPTION    - 1);
    april_detection_list_append(r, &det);
}

static void check_kernel_params(SENSEI_DETECTION_LIST *r) {
    struct { const char *path; const char *bad_val; const char *type; const char *desc; } params[] = {
        { "/proc/sys/kernel/kptr_restrict",      "0", "KPTR_EXPOSED",
          "kptr_restrict=0 — kernel pointers visible in /proc, aids rootkit development" },
        { "/proc/sys/kernel/dmesg_restrict",     "0", "DMESG_EXPOSED",
          "dmesg_restrict=0 — kernel log readable by unprivileged processes" },
        { "/proc/sys/kernel/randomize_va_space", "0", "ASLR_DISABLED",
          "ASLR disabled (randomize_va_space=0) — memory layout predictable, exploits easier" },
        { "/proc/sys/kernel/perf_event_paranoid","0", "PERF_EXPOSED",
          "perf_event_paranoid=0 — performance events accessible, side-channel risk" },
        { NULL, NULL, NULL, NULL }
    };
    for (int i = 0; params[i].path; i++) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "cat %s 2>/dev/null", params[i].path);
        char *val = rish_cmd(cmd);
        if (val) {
            val[strcspn(val, "\n")] = 0;
            if (strcmp(val, params[i].bad_val) == 0) {
                add_det(r, SENSEI_DETECTION_CLASS_KERNEL,
                        strcmp(params[i].bad_val,"0")==0 ? SENSEI_EVENT_PRIORITY_HIGH : SENSEI_EVENT_PRIORITY_MEDIUM,
                        SENSEI_MITRE_T1014, 85, params[i].type, params[i].desc);
                april_log("WARN", "CASEY_KERNEL: %s", params[i].type);
            }
            free(val);
        }
    }
}

static void check_syscall_table(SENSEI_DETECTION_LIST *r) {
    char *out = rish_cmd("grep -c sys_call_table /proc/kallsyms 2>/dev/null");
    if (out && atoi(out) > 0) {
        add_det(r, SENSEI_DETECTION_CLASS_KERNEL, SENSEI_EVENT_PRIORITY_CRITICAL,
                SENSEI_MITRE_T1014, 95, "SYSCALL_TABLE_EXPOSED",
                "sys_call_table address visible in /proc/kallsyms — rootkit could patch syscalls");
        april_log("THREAT", "CASEY_KERNEL: syscall table exposed in kallsyms");
    }
    free(out);
}

static void check_debugfs(SENSEI_DETECTION_LIST *r) {
    char *out = rish_cmd("mount 2>/dev/null | grep -c debugfs");
    if (out && atoi(out) > 0) {
        add_det(r, SENSEI_DETECTION_CLASS_KERNEL, SENSEI_EVENT_PRIORITY_HIGH,
                SENSEI_MITRE_T1014, 80, "DEBUGFS_MOUNTED",
                "debugfs is mounted — exposes kernel internals");
        april_log("WARN", "CASEY_KERNEL: debugfs mounted");
    }
    free(out);
}

static void check_kernelsu(SENSEI_DETECTION_LIST *r) {
    char *out = rish_cmd("test -e /sys/kernel/ksu && echo yes 2>/dev/null");
    if (out && strlen(out) > 1) {
        add_det(r, SENSEI_DETECTION_CLASS_ROOTKIT, SENSEI_EVENT_PRIORITY_LOW,
                SENSEI_MITRE_T1014, 50, "KERNELSU",
                "KernelSU detected — kernel-level root. Expected if intentionally installed.");
        april_log("INFO", "CASEY_KERNEL: KernelSU present (informational)");
    }
    free(out);
    char *ksud = rish_cmd("ps -A 2>/dev/null | grep -c ksud");
    if (ksud && atoi(ksud) > 0) {
        add_det(r, SENSEI_DETECTION_CLASS_ROOTKIT, SENSEI_EVENT_PRIORITY_LOW,
                SENSEI_MITRE_T1014, 50, "KERNELSU_DAEMON",
                "KernelSU daemon (ksud) running — kernel root management active");
        april_log("INFO", "CASEY_KERNEL: ksud running (informational)");
    }
    free(ksud);
}

static void check_kthread_impersonation(SENSEI_DETECTION_LIST *r) {
    char *out = rish_cmd(
        "for f in /proc/[0-9]*/status 2>/dev/null; do "
        "n=$(grep ^Name: $f | cut -f2); p=$(grep ^Pid: $f | cut -f2); "
        "pp=$(grep ^PPid: $f | cut -f2); "
        "[ -n \"$n\" ] && [ \"${n:0:1}\" = \"[\" ] && "
        "[ \"$p\" -gt 2 ] 2>/dev/null && [ \"$pp\" != \"2\" ] 2>/dev/null && "
        "echo $p $n $pp; done 2>/dev/null | head -3");
    if (out && strlen(out) > 2) {
        char desc[SENSEI_MAX_DESCRIPTION];
        snprintf(desc, sizeof(desc),
            "Kernel thread impersonation detected: %s", out);
        add_det(r, SENSEI_DETECTION_CLASS_KERNEL, SENSEI_EVENT_PRIORITY_CRITICAL,
                SENSEI_MITRE_T1036, 90, "KTHREAD_IMPERSONATION", desc);
        april_log("THREAT", "CASEY_KERNEL: Kthread impersonation: %s", out);
    }
    free(out);
}

static void check_kernel_modules(SENSEI_DETECTION_LIST *r) {
    char *mods = rish_cmd("cat /proc/modules 2>/dev/null | awk '{print $1}' | sort");
    if (!mods || strlen(mods) == 0) { free(mods); return; }

    /* First run — save baseline */
    FILE *bf = fopen(KMOD_BASELINE, "r");
    if (!bf) {
        bf = fopen(KMOD_BASELINE, "w");
        if (bf) { fprintf(bf, "%s", mods); fclose(bf); }
        april_log("INFO", "CASEY_KERNEL: Kernel module baseline recorded (%d modules)",
                  (int)(strlen(mods) > 0));
        free(mods);
        return;
    }

    /* Load baseline */
    fseek(bf, 0, SEEK_END);
    long bsz = ftell(bf); rewind(bf);
    char *baseline = malloc(bsz + 1);
    if (!baseline) { fclose(bf); free(mods); return; }
    fread(baseline, 1, bsz, bf);
    baseline[bsz] = 0;
    fclose(bf);

    /* Check for new modules */
    char *copy = strdup(mods);
    char *line = strtok(copy, "\n");
    int new_count = 0;
    while (line) {
        char modname[128] = {0};
        sscanf(line, "%127s", modname);
        if (strlen(modname) > 0 && !strstr(baseline, modname)) {
            char desc[SENSEI_MAX_DESCRIPTION];
            snprintf(desc, sizeof(desc),
                "NEW kernel module since baseline: %s", modname);
            add_det(r, SENSEI_DETECTION_CLASS_KERNEL, SENSEI_EVENT_PRIORITY_CRITICAL,
                    SENSEI_MITRE_T1014, 93, "NEW_KMODULE", desc);
            april_log("THREAT", "CASEY_KERNEL: New module since baseline: %s", modname);
            new_count++;
        }
        line = strtok(NULL, "\n");
    }
    if (new_count == 0)
        april_log("INFO", "CASEY_KERNEL: Kernel modules unchanged from baseline");

    free(copy); free(baseline); free(mods);
}

SENSEI_STATUS casey_kernel_check(SENSEI_DETECTION_LIST *results) {
    if (!results) return SENSEI_STATUS_ERROR;
    april_log("INFO", "CASEY_KERNEL: Starting kernel integrity scan...");
    check_kernel_params(results);
    check_syscall_table(results);
    check_debugfs(results);
    check_kernelsu(results);
    check_kthread_impersonation(results);
    check_kernel_modules(results);
    april_log("INFO", "CASEY_KERNEL: Kernel scan complete");
    return SENSEI_STATUS_OK;
}
