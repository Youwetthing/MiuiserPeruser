#include "april_runtime.h"
#ifndef APRIL_PLATFORM_H
#define APRIL_PLATFORM_H

#include "sensei_types.h"

#if defined(_WIN32) || defined(_WIN64)
#define APRIL_PLATFORM_WINDOWS 1
#define APRIL_PLATFORM_NAME "Windows"
#elif defined(__linux__)
#define APRIL_PLATFORM_LINUX 1
#define APRIL_PLATFORM_NAME "Linux"
#else
#error "Unsupported platform."
#endif

/* Platform abstraction API */
SENSEI_STATUS april_platform_init(void);
void april_platform_cleanup(void);

SENSEI_STATUS april_enum_processes(SENSEI_PROCESS_LIST *list);
SENSEI_STATUS april_get_process_info(uint32_t pid, SENSEI_PROCESS_INFO *info);

SENSEI_STATUS april_enum_memory_regions(uint32_t pid, SENSEI_MEMORY_REGION **regions);
SENSEI_STATUS april_read_process_memory(uint32_t pid, uint64_t address,
                                        void *buffer, size_t size,
                                        size_t *bytes_read);

SENSEI_STATUS april_check_syscall_table(SENSEI_DETECTION_LIST *detections);
SENSEI_STATUS april_check_idt(SENSEI_DETECTION_LIST *detections);

uint64_t april_get_timestamp_ns(void);
uint32_t april_get_cpu_count(void);
uint64_t april_get_total_memory(void);

SENSEI_STATUS april_elevate_privileges(void);

/* Helper functions (from platform_common.c) */
void april_process_list_free(SENSEI_PROCESS_LIST *list);
SENSEI_STATUS april_process_list_append(SENSEI_PROCESS_LIST *list, const SENSEI_PROCESS_INFO *info);
void april_memory_region_list_free(SENSEI_MEMORY_REGION *regions);
void april_detection_list_free(SENSEI_DETECTION_LIST *list);
SENSEI_STATUS april_detection_list_append(SENSEI_DETECTION_LIST *list,
                                          const SENSEI_DETECTION *detection);

#endif /* APRIL_PLATFORM_H */
