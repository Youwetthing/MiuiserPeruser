#ifndef LEO_DETECTION_H
#define LEO_DETECTION_H

#include <stdint.h>
#include <stdbool.h>
#include "sensei_types.h"

/* Config */
typedef struct {
    bool enable_memory_scan;
    bool enable_behavior_analysis;
    bool enable_network_monitor;
    bool enable_integrity_monitor;
    uint32_t scan_interval_ms;
} SENSEI_DETECTION_CONFIG;

/* Core lifecycle */
SENSEI_STATUS leo_init(const SENSEI_DETECTION_CONFIG *config);
void leo_shutdown(void);
SENSEI_STATUS leo_full_scan(SENSEI_DETECTION_LIST *results);
SENSEI_STATUS leo_scan_process(uint32_t pid, SENSEI_DETECTION_LIST *results);

/* List API */
void leo_detection_list_free(SENSEI_DETECTION_LIST *list);
SENSEI_STATUS leo_detection_list_append(
    SENSEI_DETECTION_LIST *list,
    const SENSEI_DETECTION *detection
);

/* Utils */
const char* leo_detection_class_to_string(SENSEI_DETECTION_CLASS cls);
const char* leo_status_to_string(SENSEI_STATUS status);

#endif
