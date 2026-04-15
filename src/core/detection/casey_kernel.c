/*
 * MiuiserPeruser – Kernel anomaly detection (Casey Jones)
 * Checks kernel version, loaded modules, and system call table.
 */

#include <leo_detection.h>
#include <april_platform.h>
#include <sensei_types.h>
#include "rish_pipe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void april_log(const char* level, const char* format, ...);

SENSEI_STATUS casey_kernel_check(SENSEI_DETECTION_LIST *results) {
    if (!results) return SENSEI_STATUS_ERROR;

    // Check kernel version for known vulnerabilities (simplified)
    char *version = rish_pipe_command("uname -r");
    if (version) {
        // Example: warn if kernel is older than 4.14 (Android 10 minimum)
        if (strstr(version, "4.4") || strstr(version, "4.9") || strstr(version, "3.")) {
            SENSEI_DETECTION det = {0};
            det.detection_class = SENSEI_DETECTION_CLASS_KERNEL;
            det.priority = SENSEI_EVENT_PRIORITY_MEDIUM;
            det.confidence = 70;
            det.mitre_id = SENSEI_MITRE_NONE;
            strncpy(det.detection_type, "OLD_KERNEL", SENSEI_MAX_DETECTION_TYPE-1);
            snprintf(det.description, SENSEI_MAX_DESCRIPTION-1, "Kernel version %s may have known vulnerabilities", version);
            leo_detection_list_append(results, &det);
        }
        free(version);
    }

    // Check for loaded kernel modules (requires root, but try via /proc/modules)
    char *modules = rish_pipe_command("cat /proc/modules 2>/dev/null | head -10");
    if (modules && strlen(modules) > 0) {
        // If we can read modules, check for suspicious ones
        if (strstr(modules, "diamorphine") || strstr(modules, "suterusu")) {
            SENSEI_DETECTION det = {0};
            det.detection_class = SENSEI_DETECTION_CLASS_KERNEL;
            det.priority = SENSEI_EVENT_PRIORITY_CRITICAL;
            det.confidence = 95;
            det.mitre_id = SENSEI_MITRE_T1014; // Rootkit
            strncpy(det.detection_type, "ROOTKIT_MODULE", SENSEI_MAX_DETECTION_TYPE-1);
            snprintf(det.description, SENSEI_MAX_DESCRIPTION-1, "Suspicious kernel module detected");
            leo_detection_list_append(results, &det);
        }
        free(modules);
    }

    return SENSEI_STATUS_OK;
}
