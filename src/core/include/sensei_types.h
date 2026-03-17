#ifndef SENSEI_TYPES_H
#define SENSEI_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef _WIN32
    #include <windows.h>
    #define SENSEI_EXPORT __declspec(dllexport)
    #define SENSEI_CALL   __stdcall
#else
    #define SENSEI_EXPORT __attribute__((visibility("default")))
    #define SENSEI_CALL
#endif

#define SENSEI_VERSION_MAJOR    0
#define SENSEI_VERSION_MINOR    1
#define SENSEI_VERSION_PATCH    0
#define SENSEI_VERSION_STRING   "0.1.0"

#define SENSEI_MAX_PROCESS_NAME 256
#define SENSEI_MAX_DETECTION_TYPE 64
#define SENSEI_MAX_DESCRIPTION  512
#define SENSEI_STACK_TRACE_SIZE 512
#define SENSEI_MAX_PATH         1024

#define SENSEI_THREAT_SCORE_MIN 0
#define SENSEI_THREAT_SCORE_MAX 1000

typedef enum _SENSEI_DETECTION_CLASS {
    SENSEI_DETECTION_CLASS_MEMORY   = 0,
    SENSEI_DETECTION_CLASS_HOOK     = 1,
    SENSEI_DETECTION_CLASS_BEHAVIOR = 2,
    SENSEI_DETECTION_CLASS_NETWORK  = 3,
    SENSEI_DETECTION_CLASS_KERNEL   = 4,
    SENSEI_DETECTION_CLASS_COUNT
} SENSEI_DETECTION_CLASS;

typedef enum _SENSEI_EVENT_PRIORITY {
    SENSEI_EVENT_PRIORITY_LOW      = 0,
    SENSEI_EVENT_PRIORITY_MEDIUM   = 1,
    SENSEI_EVENT_PRIORITY_HIGH     = 2,
    SENSEI_EVENT_PRIORITY_CRITICAL = 3,
    SENSEI_EVENT_PRIORITY_COUNT
} SENSEI_EVENT_PRIORITY;

typedef enum _SENSEI_KERNEL_EVENT_TYPE {
    SENSEI_KERNEL_EVENT_PROCESS_CREATE   = 0x0001,
    SENSEI_KERNEL_EVENT_PROCESS_EXIT     = 0x0002,
    SENSEI_KERNEL_EVENT_THREAD_CREATE    = 0x0003,
    SENSEI_KERNEL_EVENT_THREAD_EXIT      = 0x0004,
    SENSEI_KERNEL_EVENT_IMAGE_LOAD       = 0x0010,
    SENSEI_KERNEL_EVENT_MEMORY_ALLOC     = 0x0020,
    SENSEI_KERNEL_EVENT_MEMORY_PROTECT   = 0x0021,
    SENSEI_KERNEL_EVENT_HANDLE_CREATE    = 0x0030,
    SENSEI_KERNEL_EVENT_HANDLE_DUP       = 0x0031,
    SENSEI_KERNEL_EVENT_NETWORK_CONNECT  = 0x0040,
    SENSEI_KERNEL_EVENT_NETWORK_LISTEN   = 0x0041,
    SENSEI_KERNEL_EVENT_REGISTRY_WRITE   = 0x0050,
    SENSEI_KERNEL_EVENT_FILE_CREATE      = 0x0060,
    SENSEI_KERNEL_EVENT_HOOK_DETECTED    = 0x0100,
    SENSEI_KERNEL_EVENT_INJECTION_DETECTED = 0x0200
} SENSEI_KERNEL_EVENT_TYPE;

typedef enum _SENSEI_MITRE_TECHNIQUE {
    SENSEI_MITRE_NONE              = 0,
    SENSEI_MITRE_T1055_001,
    SENSEI_MITRE_T1055_002,
    SENSEI_MITRE_T1055_003,
    SENSEI_MITRE_T1055_004,
    SENSEI_MITRE_T1055_012,
    SENSEI_MITRE_T1014,
    SENSEI_MITRE_T1562_001,
    SENSEI_MITRE_T1134,
    SENSEI_MITRE_T1574,
    SENSEI_MITRE_T1547,
    SENSEI_MITRE_T1543,
    SENSEI_MITRE_T1068,
    SENSEI_MITRE_T1071,
    SENSEI_MITRE_T1036,
    SENSEI_MITRE_T1055,
    SENSEI_MITRE_COUNT
} SENSEI_MITRE_TECHNIQUE;

typedef enum _SENSEI_STATUS {
    SENSEI_STATUS_OK          = 0,
    SENSEI_STATUS_ERROR       = -1,
    SENSEI_STATUS_NO_MEMORY   = -2,
    SENSEI_STATUS_ACCESS_DENIED = -3,
    SENSEI_STATUS_NOT_FOUND   = -4,
    SENSEI_STATUS_TIMEOUT     = -5,
    SENSEI_STATUS_UNSUPPORTED = -6
} SENSEI_STATUS;

typedef struct _SENSEI_DETECTION {
    uint64_t        detection_id;
    uint32_t        pid;
    uint32_t        ppid;
    char            process_name[SENSEI_MAX_PROCESS_NAME];
    char            detection_type[SENSEI_MAX_DETECTION_TYPE];
    SENSEI_DETECTION_CLASS detection_class;
    uint64_t        address;
    uint32_t        threat_score;
    uint8_t         confidence;
    SENSEI_MITRE_TECHNIQUE mitre_id;
    char            description[SENSEI_MAX_DESCRIPTION];
    uint64_t        timestamp_ns;
    SENSEI_EVENT_PRIORITY priority;
    struct _SENSEI_DETECTION *next;
} SENSEI_DETECTION;

typedef struct _SENSEI_KERNEL_EVENT {
    uint32_t        event_type;
    uint32_t        pid;
    uint64_t        address;
    uint64_t        additional_info[4];
    uint8_t         stack_trace[SENSEI_STACK_TRACE_SIZE];
    uint64_t        timestamp;
} SENSEI_KERNEL_EVENT;

typedef struct _SENSEI_BEHAVIOR_PROFILE {
    uint32_t        pid;
    uint32_t        normal_syscalls[256];
    uint64_t        avg_memory_usage;
    uint32_t        avg_thread_count;
    uint32_t        typical_handles[10];
    uint64_t        network_connections[20];
    uint64_t        last_updated;
    bool            baseline_established;
} SENSEI_BEHAVIOR_PROFILE;

typedef struct _SENSEI_PROCESS_INFO {
    uint32_t        pid;
    uint32_t        ppid;
    char            name[SENSEI_MAX_PROCESS_NAME];
    char            path[SENSEI_MAX_PATH];
    uint64_t        base_address;
    uint64_t        memory_usage;
    uint32_t        thread_count;
    uint32_t        handle_count;
    uint64_t        create_time;
    bool            is_hidden;
    bool            is_elevated;
    struct _SENSEI_PROCESS_INFO *next;
} SENSEI_PROCESS_INFO;

typedef struct _SENSEI_MEMORY_REGION {
    uint64_t        base_address;
    uint64_t        size;
    uint32_t        protection;
    uint32_t        type;
    bool            is_executable;
    bool            is_writable;
    bool            has_pe_header;
    char            mapped_file[SENSEI_MAX_PATH];
    struct _SENSEI_MEMORY_REGION *next;
} SENSEI_MEMORY_REGION;

typedef struct _SENSEI_DETECTION_LIST {
    SENSEI_DETECTION  *head;
    SENSEI_DETECTION  *tail;
    uint32_t          count;
} SENSEI_DETECTION_LIST;

typedef struct _SENSEI_PROCESS_LIST {
    SENSEI_PROCESS_INFO *head;
    SENSEI_PROCESS_INFO *tail;
    uint32_t            count;
} SENSEI_PROCESS_LIST;

typedef struct _SENSEI_RING_BUFFER {
    SENSEI_KERNEL_EVENT *buffer;
    uint32_t            capacity;
    uint32_t            write_index;
    uint32_t            read_index;
} SENSEI_RING_BUFFER;

typedef struct _SENSEI_EVENT_QUEUE {
    SENSEI_DETECTION_LIST queues[SENSEI_EVENT_PRIORITY_COUNT];
    uint32_t              total_count;
    bool                  running;
} SENSEI_EVENT_QUEUE;

#endif /* SENSEI_TYPES_H */
