#ifndef SENSEI_TYPES_H
#define SENSEI_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../sqlite/sqlite3.h"

#define SENSEI_MAX_DETECTION_TYPE 64
#define SENSEI_MAX_DESCRIPTION    512
#define SENSEI_MAX_PATH           512
#define SENSEI_MAX_PROCESS_NAME   128
#define SENSEI_EVENT_PRIORITY_COUNT 4

typedef enum {
    SENSEI_STATUS_OK          =  0,
    SENSEI_STATUS_ERROR       = -1,
    SENSEI_STATUS_NO_MEMORY   = -2,
    SENSEI_STATUS_NOT_FOUND   = -3,
    SENSEI_STATUS_ACCESS_DENIED = -4,
    SENSEI_STATUS_TIMEOUT     = -5,
    SENSEI_STATUS_UNSUPPORTED = -6
} SENSEI_STATUS;

typedef enum {
    SENSEI_DETECTION_CLASS_NONE,
    SENSEI_DETECTION_CLASS_ROOTKIT,
    SENSEI_DETECTION_CLASS_MALWARE,
    SENSEI_DETECTION_CLASS_KERNEL,
    SENSEI_DETECTION_CLASS_HOOK,
    SENSEI_DETECTION_CLASS_BEHAVIOR,
    SENSEI_DETECTION_CLASS_MEMORY,
    SENSEI_DETECTION_CLASS_NETWORK
} SENSEI_DETECTION_CLASS;

typedef enum {
    SENSEI_MITRE_NONE,
    SENSEI_MITRE_T1055,
    SENSEI_MITRE_T1055_001,
    SENSEI_MITRE_T1055_002,
    SENSEI_MITRE_T1055_003,
    SENSEI_MITRE_T1055_004,
    SENSEI_MITRE_T1055_012,
    SENSEI_MITRE_T1014,
    SENSEI_MITRE_T1036,
    SENSEI_MITRE_T1562_001,
    SENSEI_MITRE_T1134,
    SENSEI_MITRE_T1574,
    SENSEI_MITRE_T1547,
    SENSEI_MITRE_T1543,
    SENSEI_MITRE_T1068,
    SENSEI_MITRE_T1071
} SENSEI_MITRE_TECHNIQUE;

typedef enum {
    SENSEI_EVENT_PRIORITY_LOW      = 0,
    SENSEI_EVENT_PRIORITY_MEDIUM   = 1,
    SENSEI_EVENT_PRIORITY_HIGH     = 2,
    SENSEI_EVENT_PRIORITY_CRITICAL = 3
} SENSEI_EVENT_PRIORITY;

typedef struct _SENSEI_DETECTION {
    uint32_t               pid;
    char                   name[128];
    SENSEI_DETECTION_CLASS detection_class;
    SENSEI_EVENT_PRIORITY  priority;
    SENSEI_MITRE_TECHNIQUE mitre_id;
    char                   detection_type[SENSEI_MAX_DETECTION_TYPE];
    char                   description[SENSEI_MAX_DESCRIPTION];
    int                    confidence;
    struct _SENSEI_DETECTION *next;
} SENSEI_DETECTION;

typedef struct {
    SENSEI_DETECTION *detections;
    uint32_t          count;
    SENSEI_DETECTION *head;
    SENSEI_DETECTION *tail;
} SENSEI_DETECTION_LIST;

typedef struct {
    SENSEI_DETECTION *head;
    SENSEI_DETECTION *tail;
    int               count;
} SENSEI_PRIORITY_QUEUE;

typedef struct {
    SENSEI_PRIORITY_QUEUE queues[SENSEI_EVENT_PRIORITY_COUNT];
    int                   total_count;
    bool                  running;
} SENSEI_EVENT_QUEUE;

typedef struct {
    int   threat_level;
    float anomaly_score;
    char  report[256];
    char  summary[256];
} ScanResult;

typedef struct _SENSEI_PROCESS_INFO {
    uint32_t pid;
    uint32_t ppid;
    char     name[128];
    char     path[512];
    char     cmdline[512];
    uint32_t uid;
    struct _SENSEI_PROCESS_INFO *next;
} SENSEI_PROCESS_INFO;

typedef struct {
    SENSEI_PROCESS_INFO *head;
    SENSEI_PROCESS_INFO *tail;
    uint32_t             count;
} SENSEI_PROCESS_LIST;

typedef struct _SENSEI_MEMORY_REGION {
    uint64_t base_address;
    uint64_t size;
    char     perms[8];
    char     mapped_file[SENSEI_MAX_PATH];
    int      is_executable;
    int      is_writable;
    struct _SENSEI_MEMORY_REGION *next;
} SENSEI_MEMORY_REGION;

typedef struct {
    int enable_memory_scan;
    int enable_hook_detection;
    int enable_behavior_analysis;
    int enable_kernel_analysis;
    int enable_network_monitor;
    int enable_integrity_monitor;
    int scan_interval_ms;
} SENSEI_DETECTION_CONFIG;

#endif /* SENSEI_TYPES_H */
