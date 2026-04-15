extern void april_log(const char* level, const char* format, ...);
/*
 * MiuiserPeruser – Behavioral anomaly detection (Donatello)
 */

#include <leo_detection.h>
#include <april_platform.h>
#include <sensei_types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <limits.h>
#include <unistd.h>

extern void april_log(const char* level, const char* format, ...);

/* Check if a process path is suspicious for its name */
static int is_suspicious_path(const char *proc_name, const char *proc_path) {
    /* Core system processes expected in standard directories */
    static const struct {
        const char *name;
        const char *expected_dir;
    } critical[] = {
        {"init", "/system/bin"},
        {"init", "/sbin"},
        {"zygote", "/system/bin"},
        {"system_server", "/system/bin"},
        {"servicemanager", "/system/bin"},
        {"vold", "/system/bin"},
        {"netd", "/system/bin"},
        {"surfaceflinger", "/system/bin"},
        {"adbd", "/sbin"},
        {NULL, NULL}
    };

    if (!proc_name || !proc_path) return 0;

    char path_copy[PATH_MAX];
    strncpy(path_copy, proc_path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';
    char *dir = dirname(path_copy);

    for (int i = 0; critical[i].name; i++) {
        if (strcmp(proc_name, critical[i].name) == 0) {
            if (strstr(dir, critical[i].expected_dir) == NULL)
                return 1; // suspicious
            break;
        }
    }
    return 0;
}

/* Detect unusual parent-child relationships */
static int is_unusual_parent(const char *child, const char *parent) {
    if (strcmp(parent, "init") == 0 && strcmp(child, "zygote") == 0) return 0;
    if (strcmp(parent, "init") == 0 && strcmp(child, "sh") == 0) return 0;
    if (strcmp(parent, "zygote") == 0 && strstr(child, "app_") != NULL) return 0;
    if (strcmp(parent, "sh") == 0 || strcmp(parent, "bash") == 0) {
        if (strcmp(child, "init") == 0 || strcmp(child, "zygote") == 0 ||
            strcmp(child, "system_server") == 0)
            return 1;
    }
    if (strstr(parent, "chrome") || strstr(parent, "firefox") ||
        strstr(parent, "opera") || strstr(parent, "safari")) {
        if (strcmp(child, "sh") == 0 || strcmp(child, "bash") == 0)
            return 1;
    }
    return 0;
}

/* Check for hidden process (exists in /proc but can't read status) */
static int is_hidden_process(uint32_t pid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%u/status", pid);
    if (access(path, F_OK) == 0 && access(path, R_OK) != 0)
        return 1;
    return 0;
}

SENSEI_STATUS don_behavior_analyze(uint32_t pid, SENSEI_DETECTION_LIST *results) {
    if (!results) return SENSEI_STATUS_ERROR;

    /* Hidden process check */
    if (is_hidden_process(pid)) {
        SENSEI_DETECTION det = {0};
        det.pid = pid;
        det.detection_class = SENSEI_DETECTION_CLASS_BEHAVIOR;
        det.priority = SENSEI_EVENT_PRIORITY_CRITICAL;
        det.confidence = 99;
        det.mitre_id = SENSEI_MITRE_T1014; // Rootkit
        strncpy(det.detection_type, "HIDDEN_PROCESS", SENSEI_MAX_DETECTION_TYPE - 1);
        snprintf(det.description, SENSEI_MAX_DESCRIPTION,
                 "Process PID %u appears hidden (cannot read status)", pid);
        leo_detection_list_append(results, &det);
    static int behavior_scan_count = 0;
    behavior_scan_count++;
    if (behavior_scan_count % 10 == 0) april_log("INFO", "Behavior scan completed (no hidden/suspicious processes)");
        return SENSEI_STATUS_OK;
    }

    SENSEI_PROCESS_INFO info;
    if (april_get_process_info(pid, &info) != SENSEI_STATUS_OK)
        return SENSEI_STATUS_ERROR;

    /* Suspicious path */
    if (is_suspicious_path(info.name, info.path)) {
        SENSEI_DETECTION det = {0};
        det.pid = pid;
        det.detection_class = SENSEI_DETECTION_CLASS_BEHAVIOR;
        det.priority = SENSEI_EVENT_PRIORITY_HIGH;
        det.confidence = 85;
        det.mitre_id = SENSEI_MITRE_T1036; // Masquerading
        strncpy(det.detection_type, "SUSPICIOUS_PATH", SENSEI_MAX_DETECTION_TYPE - 1);
        snprintf(det.description, SENSEI_MAX_DESCRIPTION,
                 "'%s' running from unexpected path: %s", info.name, info.path);
        leo_detection_list_append(results, &det);
    }

    /* Unusual parent */
    if (info.ppid > 0) {
        SENSEI_PROCESS_INFO parent;
        if (april_get_process_info(info.ppid, &parent) == SENSEI_STATUS_OK) {
            if (is_unusual_parent(info.name, parent.name)) {
                SENSEI_DETECTION det = {0};
                det.pid = pid;
                det.detection_class = SENSEI_DETECTION_CLASS_BEHAVIOR;
                det.priority = SENSEI_EVENT_PRIORITY_MEDIUM;
                det.confidence = 75;
                det.mitre_id = SENSEI_MITRE_T1055; // Process Injection
                strncpy(det.detection_type, "UNUSUAL_PARENT", SENSEI_MAX_DETECTION_TYPE - 1);
                snprintf(det.description, SENSEI_MAX_DESCRIPTION,
                         "'%s' (PID %u) has unusual parent '%s' (PID %u)",
                         info.name, pid, parent.name, info.ppid);
                leo_detection_list_append(results, &det);
            }
        }
    }

    static int behavior_scan_count = 0;
    behavior_scan_count++;
    if (behavior_scan_count % 10 == 0) april_log("INFO", "Behavior scan completed (no hidden/suspicious processes)");
    return SENSEI_STATUS_OK;
}
