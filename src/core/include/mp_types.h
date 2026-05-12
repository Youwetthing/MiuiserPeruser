#ifndef MP_TYPES_H
#define MP_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/* Status codes */
typedef int mp_status_t;
#define MP_OK           0
#define MP_ERROR       -1
#define MP_NO_MEMORY   -2
#define MP_ACCESS_DENIED -3
#define MP_NOT_FOUND   -4
#define MP_TIMEOUT     -5
#define MP_UNSUPPORTED -6

/* Detection classes */
typedef enum {
    MP_CLASS_MEMORY   = 0,
    MP_CLASS_HOOK     = 1,
    MP_CLASS_BEHAVIOR = 2,
    MP_CLASS_NETWORK  = 3,
    MP_CLASS_KERNEL   = 4,
    MP_CLASS_COUNT
} mp_detection_class_t;

/* Event priority */
typedef enum {
    MP_PRIORITY_LOW      = 0,
    MP_PRIORITY_MEDIUM   = 1,
    MP_PRIORITY_HIGH     = 2,
    MP_PRIORITY_CRITICAL = 3,
    MP_PRIORITY_COUNT
} mp_priority_t;

#define MP_MAX_PROCESS_NAME  256
#define MP_MAX_DETECTION_TYPE 64
#define MP_MAX_DESCRIPTION   512
#define MP_MAX_PATH         1024
#define MP_THREAT_SCORE_MAX 1000

/* Detection event */
typedef struct _mp_detection_t {
    uint64_t             id;
    uint32_t             pid;
    uint32_t             ppid;
    char                 process_name[MP_MAX_PROCESS_NAME];
    char                 detection_type[MP_MAX_DETECTION_TYPE];
    mp_detection_class_t detection_class;
    uint32_t             threat_score;
    uint8_t              confidence;
    char                 description[MP_MAX_DESCRIPTION];
    uint64_t             timestamp_ns;
    mp_priority_t        priority;
    struct _mp_detection_t *next;
} mp_detection_t;

/* Detection list */
typedef struct {
    mp_detection_t *head;
    mp_detection_t *tail;
    uint32_t        count;
} mp_detection_list_t;

/* Process info */
typedef struct _mp_process_info_t {
    uint32_t pid;
    uint32_t ppid;
    char     name[MP_MAX_PROCESS_NAME];
    char     path[MP_MAX_PATH];
    uint64_t memory_usage;
    uint32_t thread_count;
    bool     is_hidden;
    struct _mp_process_info_t *next;
} mp_process_info_t;

/* Process list */
typedef struct {
    mp_process_info_t *head;
    mp_process_info_t *tail;
    uint32_t           count;
} mp_process_list_t;

/* Detection config */
typedef struct {
    bool enable_memory_scan;
    bool enable_hook_detection;
    bool enable_behavior_analysis;
    bool enable_kernel_analysis;
    bool enable_network_monitor;
    bool enable_integrity_monitor;
    int  scan_interval_ms;
} mp_detection_config_t;

#endif /* MP_TYPES_H */

/* Memory region */
typedef struct _mp_memory_region_t {
    uint64_t base_address;
    uint64_t size;
    uint32_t protection;
    bool     is_executable;
    bool     is_writable;
    bool     has_pe_header;
    char     mapped_file[MP_MAX_PATH];
    struct _mp_memory_region_t *next;
} mp_memory_region_t;

/* Event queue */
typedef struct {
    mp_detection_list_t queues[MP_PRIORITY_COUNT];
    uint32_t            total_count;
    bool                running;
} mp_event_queue_t;
