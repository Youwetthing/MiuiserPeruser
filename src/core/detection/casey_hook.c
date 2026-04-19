#include "compat/sensei_compat.h"
#include <leo_detection.h>
#include <sensei_types.h>
#include "rish_pipe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void april_log(const char* level, const char* format, ...);

SENSEI_STATUS casey_hook_check(SENSEI_DETECTION_LIST *results) {
    if (!results) return SENSEI_STATUS_ERROR;

    // ANOMALY: Reflective Injection / Fileless Execution
    char *maps = rish_pipe_command("grep 'r-xp' /proc/self/maps | grep -v '/'");
    if (maps && strlen(maps) > 0) {
        SENSEI_DETECTION det = {0};
        det.detection_class = SENSEI_DETECTION_CLASS_HOOK;
        det.priority = SENSEI_EVENT_PRIORITY_CRITICAL;
        det.confidence = 98;
        det.mitre_id = SENSEI_MITRE_T1055;
        strncpy(det.detection_type, "ANON_EXEC_MEM", SENSEI_MAX_DETECTION_TYPE-1);
        snprintf(det.description, SENSEI_MAX_DESCRIPTION-1, "Anonymous executable memory detected (Reflective Hook)");
        april_detection_list_append(results, &det);
        april_log("THREAT", "BEHAVIOR: Anonymous code execution bridge detected");
    }
    free(maps);

    char *preload = rish_pipe_command("grep 'LD_PRELOAD' /proc/1/environ | tr '\\0' '\\n'");
    if (preload && strlen(preload) > 0) {
        april_log("WARN", "Persistence: LD_PRELOAD active on init");
    }
    free(preload);

    return SENSEI_STATUS_OK;
}
