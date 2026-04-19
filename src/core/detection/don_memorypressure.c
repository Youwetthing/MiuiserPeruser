#include "compat/sensei_compat.h"
/*
 * MiuiserPeruser – Donatello's Memory Pressure Monitor
 * Watches RAM and swap usage, warns when resources are low.
 */

#include <leo_detection.h>
#include <sensei_types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern void april_log(const char* level, const char* format, ...);

// Thresholds (percent)
#define RAM_CRITICAL 85   // Available RAM less than 15%? Actually we want low available, so...
#define SWAP_HIGH    80   // Swap usage above 80%

// We'll track previous values to avoid repeated logs
static unsigned long long last_ram_warning = 0;
static unsigned long long last_swap_warning = 0;

/* Parse /proc/meminfo for a specific key */
static unsigned long long get_mem_value(const char *key) {
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) return 0;
    char line[256];
    unsigned long long val = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, key, strlen(key)) == 0) {
            sscanf(line, "%*s %llu", &val);
            break;
        }
    }
    fclose(fp);
    return val;
}

SENSEI_STATUS don_memorypressure_check(SENSEI_DETECTION_LIST *results) {
    (void)results;  // Not adding detections, just logging

    unsigned long long mem_total = get_mem_value("MemTotal:");
    unsigned long long mem_avail = get_mem_value("MemAvailable:");
    unsigned long long swap_total = get_mem_value("SwapTotal:");
    unsigned long long swap_free = get_mem_value("SwapFree:");

    if (mem_total == 0) {
        // /proc/meminfo not readable? shouldn't happen.
        return SENSEI_STATUS_OK;
    }

    // Calculate percentages
    int ram_used_percent = 100 - (mem_avail * 100 / mem_total);
    int swap_used_percent = (swap_total > 0) ? ( (swap_total - swap_free) * 100 / swap_total ) : 0;

    // Log current status occasionally (e.g., every 10 scans) to keep user informed
    static int counter = 0;
    counter++;
    if (counter % 10 == 0) {
        april_log("MEMORY", "RAM used: %d%%, swap used: %d%%", ram_used_percent, swap_used_percent);
    }

    // Critical RAM warning
    if (ram_used_percent >= RAM_CRITICAL) {
        if (last_ram_warning == 0 || last_ram_warning + 5 < counter) {  // warn at most every 5 scans
            april_log("THREAT", "Woah Nelly! RAM critically low (%d%% used). MIUI may start killing processes.", ram_used_percent);
            last_ram_warning = counter;
        }
    }

    // High swap usage warning
    if (swap_used_percent >= SWAP_HIGH) {
        if (last_swap_warning == 0 || last_swap_warning + 5 < counter) {
            april_log("WARN", "Swap usage high (%d%%) – system is paging heavily.", swap_used_percent);
            last_swap_warning = counter;
        }
    }

    return SENSEI_STATUS_OK;
}
