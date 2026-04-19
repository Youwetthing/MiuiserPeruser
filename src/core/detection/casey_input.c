#include "compat/sensei_compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rish_pipe.h"
#include "sensei_types.h"
#include "leo_detection.h"

ScanResult scan_casey_input(void) {
    ScanResult result = {0};
    char *services = rish_pipe_command("settings get secure enabled_accessibility_services");

    if (services && strlen(services) > 0 && strcmp(services, "null") != 0) {
        if (strstr(services, "com.miui.") || strstr(services, "com.xiaomi.")) {
            result.threat_level = 1;
            result.anomaly_score = 85; // High score for MIUI scrapers
            strncpy(result.summary, "MIUI Scraper Active", 255);
        } else {
            result.anomaly_score = 0;
            strncpy(result.summary, "Clear", 255);
        }
    }
    
    if (services) free(services);
    return result;
}
