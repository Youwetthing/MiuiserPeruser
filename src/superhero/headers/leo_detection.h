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

/* Core API */
SENSEI_STATUS leo_init(const SENSEI_DETECTION_CONFIG *config);
void leo_shutdown(void);
SENSEI_STATUS leo_full_scan(SENSEI_DETECTION_LIST *results);

/* Brothers & Utils */
SENSEI_STATUS raph_network_scan(SENSEI_DETECTION_LIST *results);
SENSEI_STATUS raph_memory_scan(int pid, SENSEI_DETECTION_LIST *results);
SENSEI_STATUS don_memorypressure_check(SENSEI_DETECTION_LIST *results);
SENSEI_STATUS don_behavior_analyze(int pid, SENSEI_DETECTION_LIST *results);
SENSEI_STATUS mikey_miui(SENSEI_DETECTION_LIST *results);

/* April's Linux Platform */
SENSEI_STATUS april_enum_processes(SENSEI_PROCESS_LIST *list);
void april_process_list_free(SENSEI_PROCESS_LIST *list);

/* List Management */
void leo_detection_list_free(SENSEI_DETECTION_LIST *list);
SENSEI_STATUS leo_detection_list_append(SENSEI_DETECTION_LIST *list, SENSEI_DETECTION *detection);

#endif
