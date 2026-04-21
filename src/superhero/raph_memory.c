/*
 * MiuiserPeruser – Memory anomaly detection (Raphael)
 */

#include <leo_detection.h>
#include <april_platform.h>
#include <sensei_types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

/* Check if region is RWX (Raphael's punch) */
static bool is_rwx_region(const SENSEI_MEMORY_REGION *region) {
    return region->is_executable && region->is_writable;
}

/* Check for reflective code (ELF header in anonymous mapping) */
static bool is_reflective_code(const SENSEI_MEMORY_REGION *region) {
    if (!region->has_pe_header) return false;
    return region->mapped_file[0] == '\0';
}

SENSEI_STATUS raph_memory_scan(uint32_t pid, SENSEI_DETECTION_LIST *results) {
    if (!results) return SENSEI_STATUS_ERROR;

    SENSEI_MEMORY_REGION *regions = NULL;
    if (april_enum_memory_regions(pid, &regions) != SENSEI_STATUS_OK)
        return SENSEI_STATUS_ERROR;

    SENSEI_MEMORY_REGION *cur = regions;
    while (cur) {
        /* RWX pages – shellcode */
        if (is_rwx_region(cur)) {
            SENSEI_DETECTION det = {0};
            det.pid = pid;
            det.detection_class = SENSEI_DETECTION_CLASS_MEMORY;
            det.priority = SENSEI_EVENT_PRIORITY_HIGH;
            det.confidence = 80;
            det.address = cur->base_address;
            det.mitre_id = SENSEI_MITRE_T1055_001; // Process Injection
            strncpy(det.detection_type, "RWX_MEMORY_PAGE", SENSEI_MAX_DETECTION_TYPE - 1);
            snprintf(det.description, SENSEI_MAX_DESCRIPTION,
                     "RWX memory at 0x%" PRIx64 " (size %" PRIu64 " bytes)",
                     cur->base_address, cur->size);
            leo_detection_list_append(results, &det);
        }

        /* Reflective code – ELF header in anonymous mapping */
        if (is_reflective_code(cur)) {
            SENSEI_DETECTION det = {0};
            det.pid = pid;
            det.detection_class = SENSEI_DETECTION_CLASS_MEMORY;
            det.priority = SENSEI_EVENT_PRIORITY_CRITICAL;
            det.confidence = 95;
            det.address = cur->base_address;
            det.mitre_id = SENSEI_MITRE_T1055_002; // PE/ELF Injection
            strncpy(det.detection_type, "REFLECTIVE_CODE", SENSEI_MAX_DETECTION_TYPE - 1);
            snprintf(det.description, SENSEI_MAX_DESCRIPTION,
                     "ELF header in anonymous executable memory at 0x%" PRIx64,
                     cur->base_address);
            leo_detection_list_append(results, &det);
        }
        cur = cur->next;
    }

    april_memory_region_list_free(regions);
    return SENSEI_STATUS_OK;
}
