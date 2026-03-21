#ifndef LEO_DETECTION_H
#define LEO_DETECTION_H

#include <stdint.h>
#include <stdbool.h>
#include "sensei_types.h"

typedef struct _SENSEI_DETECTION_CONFIG {
    bool enable_memory_scan;
    bool enable_hook_detection;
    bool enable_behavior_analysis;
    bool enable_kernel_analysis;
    bool enable_network_monitor;
    bool enable_integrity_monitor;
    uint32_t scan_interval_ms;
} SENSEI_DETECTION_CONFIG;

SENSEI_STATUS leo_init(const SENSEI_DETECTION_CONFIG *config);
void leo_shutdown(void);
SENSEI_STATUS leo_full_scan(SENSEI_DETECTION_LIST *results);
SENSEI_STATUS leo_scan_process(uint32_t pid, SENSEI_DETECTION_LIST *results);

void leo_detection_list_free(SENSEI_DETECTION_LIST *list);
SENSEI_STATUS leo_detection_list_append(SENSEI_DETECTION_LIST *list,
                                        const SENSEI_DETECTION *detection);

const char* leo_detection_class_to_string(SENSEI_DETECTION_CLASS cls);
const char* leo_mitre_technique_to_string(SENSEI_MITRE_TECHNIQUE technique);
const char* leo_event_priority_to_string(SENSEI_EVENT_PRIORITY priority);
const char* leo_status_to_string(SENSEI_STATUS status);
uint32_t leo_calculate_score(const SENSEI_DETECTION *detection);
SENSEI_MITRE_TECHNIQUE leo_map_mitre(const SENSEI_DETECTION *detection);

#endif /* LEO_DETECTION_H */
SENSEI_STATUS don_memorypressure_check(SENSEI_DETECTION_LIST *results);
