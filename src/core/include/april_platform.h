#ifndef APRIL_PLATFORM_H
#define APRIL_PLATFORM_H

#include "sensei_types.h"

/* Platform abstraction */
SENSEI_STATUS april_platform_init(void);
void april_platform_cleanup(void);

SENSEI_STATUS april_enum_processes(SENSEI_DETECTION_LIST *list);
SENSEI_STATUS april_get_process_info(uint32_t pid, SENSEI_PROCESS_INFO *info);

SENSEI_STATUS april_enum_memory_regions(uint32_t pid, SENSEI_DETECTION_LIST *results);
SENSEI_STATUS april_read_process_memory(uint32_t pid, uint64_t address, void *buffer, size_t size, size_t *bytes_read);

SENSEI_STATUS april_check_syscall_table(SENSEI_DETECTION_LIST *results);
SENSEI_STATUS april_check_idt(SENSEI_DETECTION_LIST *results);

void april_process_list_free(SENSEI_DETECTION_LIST *list);
SENSEI_STATUS april_detection_list_append(SENSEI_DETECTION_LIST *list, const SENSEI_DETECTION *detection);
void april_detection_list_free(SENSEI_DETECTION_LIST *list);

#endif
