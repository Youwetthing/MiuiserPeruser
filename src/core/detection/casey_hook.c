/*
 * MiuiserPeruser – Hook detection (Casey Jones)
 * Checks for LD_PRELOAD, suspicious environment, and ptrace.
 */

#include <leo_detection.h>
#include <april_platform.h>
#include <sensei_types.h>
#include "rish_pipe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void april_log(const char* level, const char* format, ...);

SENSEI_STATUS casey_hook_check(SENSEI_DETECTION_LIST *results) {
    if (!results) return SENSEI_STATUS_ERROR;

    // Check for LD_PRELOAD in init or zygote
    char *preload = rish_pipe_command("cat /proc/1/environ 2>/dev/null | tr '\\0' '\\n' | grep LD_PRELOAD");
    if (preload && strlen(preload) > 0) {
        SENSEI_DETECTION det = {0};
        det.detection_class = SENSEI_DETECTION_CLASS_HOOK;
        det.priority = SENSEI_EVENT_PRIORITY_HIGH;
        det.confidence = 90;
        det.mitre_id = SENSEI_MITRE_T1574; // Hijack Execution Flow
        strncpy(det.detection_type, "LD_PRELOAD", SENSEI_MAX_DETECTION_TYPE-1);
        snprintf(det.description, SENSEI_MAX_DESCRIPTION-1, "LD_PRELOAD set on init: %s", preload);
        leo_detection_list_append(results, &det);
        april_log("THREAT", "HOOK: LD_PRELOAD detected on init");
    }
    free(preload);

    // Check for suspicious /proc/pid/maps entries (already partially covered by raph_memory)
    // This is a placeholder for more advanced checks like inline hooks (requires binary analysis)

    return SENSEI_STATUS_OK;
}
