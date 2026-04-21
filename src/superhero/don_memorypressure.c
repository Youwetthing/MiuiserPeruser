#include <leo_detection.h>
#include <sensei_types.h>
#include <stdio.h>
#include <string.h>

SENSEI_STATUS don_memorypressure_check(SENSEI_DETECTION_LIST *results) {
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) return SENSEI_STATUS_ERROR;

    unsigned long avail = 0;
    char line[128];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "MemAvailable:", 13) == 0) {
            sscanf(line + 13, "%lu", &avail);
            break;
        }
    }
    fclose(fp);

    /* Under 64MB? HyperOS is looking for someone to kill. */
    if (avail < 65536) { 
        SENSEI_DETECTION det = {
            .priority = SENSEI_EVENT_PRIORITY_CRITICAL,
            .confidence = 90,
            .description = "System-wide RAM exhaustion. Termux daemon at risk of OOM-kill."
        };
        strncpy(det.detection_type, "MEM_PRESSURE_CRITICAL", 63);
        leo_detection_list_append(results, &det);
    }
    return SENSEI_STATUS_OK;
}
