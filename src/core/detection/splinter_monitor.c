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

#include "backend_mask.h"

int splinter_status_check(void)
{
    const backend_mask_t *mask = backend_get_mask();

    if (!mask)
        return 0;

    return (mask->active != BACKEND_NONE);
}
