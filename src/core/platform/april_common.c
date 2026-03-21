#include <april_platform.h>
#include <leo_detection.h>
#include <sensei_types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Detection List Management */
static SENSEI_DETECTION *sensei_detection_alloc(void) {
    return (SENSEI_DETECTION *)calloc(1, sizeof(SENSEI_DETECTION));
}

void april_detection_list_free(SENSEI_DETECTION_LIST *list) {
    if (!list) return;
    SENSEI_DETECTION *cur = list->head;
    while (cur) {
        SENSEI_DETECTION *next = cur->next;
        free(cur);
        cur = next;
    }
    list->head = list->tail = NULL;
    list->count = 0;
}

SENSEI_STATUS april_detection_list_append(SENSEI_DETECTION_LIST *list,
                                          const SENSEI_DETECTION *detection) {
    if (!list || !detection) return SENSEI_STATUS_ERROR;
    SENSEI_DETECTION *copy = sensei_detection_alloc();
    if (!copy) return SENSEI_STATUS_NO_MEMORY;
    memcpy(copy, detection, sizeof(SENSEI_DETECTION));
    copy->next = NULL;
    if (!list->head) {
        list->head = copy;
        list->tail = copy;
    } else {
        list->tail->next = copy;
        list->tail = copy;
    }
    list->count++;
    return SENSEI_STATUS_OK;
}

/* Process List Management */
void april_process_list_free(SENSEI_PROCESS_LIST *list) {
    if (!list) return;
    SENSEI_PROCESS_INFO *cur = list->head;
    while (cur) {
        SENSEI_PROCESS_INFO *next = cur->next;
        free(cur);
        cur = next;
    }
    list->head = list->tail = NULL;
    list->count = 0;
}

SENSEI_STATUS april_process_list_append(SENSEI_PROCESS_LIST *list,
                                        const SENSEI_PROCESS_INFO *info) {
    if (!list || !info) return SENSEI_STATUS_ERROR;
    SENSEI_PROCESS_INFO *copy = calloc(1, sizeof(SENSEI_PROCESS_INFO));
    if (!copy) return SENSEI_STATUS_NO_MEMORY;
    memcpy(copy, info, sizeof(SENSEI_PROCESS_INFO));
    copy->next = NULL;
    if (!list->head) {
        list->head = copy;
        list->tail = copy;
    } else {
        list->tail->next = copy;
        list->tail = copy;
    }
    list->count++;
    return SENSEI_STATUS_OK;
}

/* Memory Region List Management */
void april_memory_region_list_free(SENSEI_MEMORY_REGION *regions) {
    SENSEI_MEMORY_REGION *cur = regions;
    while (cur) {
        SENSEI_MEMORY_REGION *next = cur->next;
        free(cur);
        cur = next;
    }
}

/* String Conversion Utilities */
const char* leo_detection_class_to_string(SENSEI_DETECTION_CLASS cls) {
    switch (cls) {
        case SENSEI_DETECTION_CLASS_MEMORY:   return "Memory";
        case SENSEI_DETECTION_CLASS_HOOK:     return "Hook";
        case SENSEI_DETECTION_CLASS_BEHAVIOR: return "Behavior";
        case SENSEI_DETECTION_CLASS_NETWORK:  return "Network";
        case SENSEI_DETECTION_CLASS_KERNEL:   return "Kernel";
        default: return "Unknown";
    }
}

const char* leo_mitre_technique_to_string(SENSEI_MITRE_TECHNIQUE tech) {
    switch (tech) {
        case SENSEI_MITRE_NONE:        return "N/A";
        case SENSEI_MITRE_T1055_001:   return "T1055.001 - DLL Injection";
        case SENSEI_MITRE_T1055_002:   return "T1055.002 - PE Injection";
        case SENSEI_MITRE_T1055_003:   return "T1055.003 - Thread Hijacking";
        case SENSEI_MITRE_T1055_004:   return "T1055.004 - APC Injection";
        case SENSEI_MITRE_T1055_012:   return "T1055.012 - Process Hollowing";
        case SENSEI_MITRE_T1014:       return "T1014 - Rootkit";
        case SENSEI_MITRE_T1562_001:   return "T1562.001 - Disable Tools";
        case SENSEI_MITRE_T1134:       return "T1134 - Token Manipulation";
        case SENSEI_MITRE_T1574:       return "T1574 - Hijack Execution Flow";
        case SENSEI_MITRE_T1547:       return "T1547 - Autostart";
        case SENSEI_MITRE_T1543:       return "T1543 - Create System Process";
        case SENSEI_MITRE_T1068:       return "T1068 - Priv Escalation";
        case SENSEI_MITRE_T1071:       return "T1071 - App Layer Protocol";
        case SENSEI_MITRE_T1036:       return "T1036 - Masquerading";
        case SENSEI_MITRE_T1055:       return "T1055 - Process Injection";
        default: return "Unknown";
    }
}

const char* leo_event_priority_to_string(SENSEI_EVENT_PRIORITY prio) {
    switch (prio) {
        case SENSEI_EVENT_PRIORITY_LOW:      return "Low";
        case SENSEI_EVENT_PRIORITY_MEDIUM:   return "Medium";
        case SENSEI_EVENT_PRIORITY_HIGH:     return "High";
        case SENSEI_EVENT_PRIORITY_CRITICAL: return "Critical";
        default: return "Unknown";
    }
}

const char* leo_status_to_string(SENSEI_STATUS status) {
    switch (status) {
        case SENSEI_STATUS_OK:           return "OK";
        case SENSEI_STATUS_ERROR:        return "Error";
        case SENSEI_STATUS_NO_MEMORY:    return "Out of memory";
        case SENSEI_STATUS_ACCESS_DENIED: return "Access denied";
        case SENSEI_STATUS_NOT_FOUND:    return "Not found";
        case SENSEI_STATUS_TIMEOUT:      return "Timeout";
        case SENSEI_STATUS_UNSUPPORTED:  return "Unsupported";
        default: return "Unknown";
    }
}
