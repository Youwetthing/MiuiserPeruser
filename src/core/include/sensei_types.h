#ifndef SENSEI_TYPES_H
#define SENSEI_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sqlite3.h>

#define SENSEI_MAX_DETECTION_TYPE 64
#define SENSEI_MAX_DESCRIPTION 512

/* Status */
typedef enum {
    SENSEI_STATUS_OK = 0,
    SENSEI_STATUS_ERROR = -1
} SENSEI_STATUS;

/* Detection classes */
typedef enum {
    SENSEI_DETECTION_CLASS_NONE,
    SENSEI_DETECTION_CLASS_ROOTKIT,
    SENSEI_DETECTION_CLASS_MALWARE,
    SENSEI_DETECTION_CLASS_KERNEL,
    SENSEI_DETECTION_CLASS_HOOK,
    SENSEI_DETECTION_CLASS_BEHAVIOR
} SENSEI_DETECTION_CLASS;

/* MITRE */
typedef enum {
    SENSEI_MITRE_NONE,
    SENSEI_MITRE_T1055,
    SENSEI_MITRE_T1014
} SENSEI_MITRE_TECHNIQUE;

/* Priority */
typedef enum {
    SENSEI_EVENT_PRIORITY_LOW,
    SENSEI_EVENT_PRIORITY_MEDIUM,
    SENSEI_EVENT_PRIORITY_HIGH,
    SENSEI_EVENT_PRIORITY_CRITICAL
} SENSEI_EVENT_PRIORITY;

/* Main detection struct */
typedef struct {
    uint32_t pid;
    char name[128];
    SENSEI_DETECTION_CLASS detection_class;
    SENSEI_EVENT_PRIORITY priority;
    SENSEI_MITRE_TECHNIQUE mitre_id;
    char detection_type[SENSEI_MAX_DETECTION_TYPE];
    char description[SENSEI_MAX_DESCRIPTION];
    int confidence;
} SENSEI_DETECTION;

/* Lists */
typedef struct SENSEI_DETECTION_LIST {
    SENSEI_DETECTION *detections;
    uint32_t count;
} SENSEI_DETECTION_LIST;

typedef struct {
    int threat_level;
    int anomaly_score;
} ScanResult;

/* Config */
typedef struct {
    int enable_memory_scan;
    int enable_hook_detection;
    int enable_behavior_analysis;
    int enable_kernel_analysis;
    int enable_network_monitor;
    int enable_integrity_monitor;
    int scan_interval_ms;
} SENSEI_DETECTION_CONFIG;

#endif
