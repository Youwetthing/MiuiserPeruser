#include "compat/sensei_compat.h"
#include "sensei_types.h"
#include "april_platform.h"
#include "rish_pipe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern void april_log(const char* level, const char* format, ...);
extern int april_detection_list_append(SENSEI_DETECTION_LIST *list, const SENSEI_DETECTION *det);
extern SENSEI_STATUS april_get_process_info(uint32_t pid, SENSEI_PROCESS_INFO *info);
extern SENSEI_STATUS april_enum_memory_regions(uint32_t pid, SENSEI_MEMORY_REGION **regions);

#include "compat/sensei_compat.h"
#include "syndicate_db.h"
#include <stdio.h>

void fugitoid_log_event(const char* event) {
    // Fugitoid uses the April Shim symbols to record cross-daemon trends
    syndicate_db_log("FUGITOID", "ANALYSIS", event);
}
