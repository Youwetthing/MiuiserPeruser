#include "compat/sensei_compat.h"
#include <leo_detection.h>
#include <sensei_types.h>
#include "rish_pipe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void april_log(const char* level, const char* format, ...);

SENSEI_STATUS casey_kernel_check(SENSEI_DETECTION_LIST *results) {
    if (!results) return SENSEI_STATUS_ERROR;

    // ANOMALY: Kernel Pointer Leakage
    char *kptr = rish_pipe_command("cat /proc/sys/kernel/kptr_restrict");
    if (kptr && atoi(kptr) == 0) {
        SENSEI_DETECTION det = {0};
        det.detection_class = SENSEI_DETECTION_CLASS_KERNEL;
        det.priority = SENSEI_EVENT_PRIORITY_HIGH;
        det.confidence = 85;
        strncpy(det.detection_type, "KPTR_LEAK", SENSEI_MAX_DETECTION_TYPE-1);
        snprintf(det.description, SENSEI_MAX_DESCRIPTION-1, "Kernel pointer restriction is DISABLED.");
        april_detection_list_append(results, &det);
    }
    free(kptr);

    // ANOMALY: Syscall Table Exposure
    char *kallsyms = rish_pipe_command("grep -E 'sys_call_table|wp_detect' /proc/kallsyms 2>/dev/null");
    if (kallsyms && strlen(kallsyms) > 0) {
        SENSEI_DETECTION det = {0};
        det.detection_class = SENSEI_DETECTION_CLASS_KERNEL;
        det.priority = SENSEI_EVENT_PRIORITY_CRITICAL;
        det.confidence = 95;
        det.mitre_id = SENSEI_MITRE_T1014;
        strncpy(det.detection_type, "SYS_TABLE_EXPOSED", SENSEI_MAX_DETECTION_TYPE-1);
        snprintf(det.description, SENSEI_MAX_DESCRIPTION-1, "System call table exposed in kallsyms.");
        april_detection_list_append(results, &det);
        april_log("THREAT", "KERNEL: Potential Rootkit preparation detected");
    }
    free(kallsyms);

    return SENSEI_STATUS_OK;
}
