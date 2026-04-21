#include "leo_detection.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void fugitoid_log(const char *message);
SENSEI_STATUS rish_pipe_command(const char* cmd, char* result, size_t size);

SENSEI_STATUS leo_init(const SENSEI_DETECTION_CONFIG *config) {
    (void)config;
    return SENSEI_STATUS_OK;
}

void leo_shutdown(void) {}

static void parse_batch_output(const char *output, SENSEI_DETECTION_LIST *results) {
    // HEARTBEAT CHECK: If 'uid=' isn't there, the pipe failed
    if (!strstr(output, "uid=")) {
        SENSEI_DETECTION d = {0};
        strncpy(d.description, "ERROR: Forensic pipe failure (System Gaslighting?)", 127);
        leo_detection_list_append(results, &d);
        return;
    }

    if (strstr(output, "uid=2000(shell)")) printf("[LEO] Privilege: ELEVATED\n");

    // 1. PUPPET MASTER: Accessibility Check
    if (strstr(output, "enabled_accessibility_services")) {
        char *start = strstr(output, "enabled_accessibility_services");
        // If the services string is longer than the label and not just 'null'
        if (strlen(start) > 32 && !strstr(start, "null") && !strstr(start, "invalid")) {
             // Basic whitelist
             if (!strstr(start, "com.google") && !strstr(start, "moe.shizuku")) {
                SENSEI_DETECTION d = {0};
                strncpy(d.description, "WARNING: External Accessibility Service is ACTIVE", 127);
                leo_detection_list_append(results, &d);
             }
        }
    }

    // 2. SHIZUKU CHECK (The Heartbeat)
    if (strstr(output, "moe.shizuku.privileged.api")) {
        SENSEI_DETECTION d = {0};
        strncpy(d.description, "INFO: Shizuku Context Active", 127);
        leo_detection_list_append(results, &d);
    }
}

SENSEI_STATUS leo_full_scan(SENSEI_DETECTION_LIST *results) {
    char *big_buffer = calloc(1, 262144);
    if (!big_buffer) return SENSEI_STATUS_NO_MEMORY;

    // Explicitly command 'id' first to verify the pipe is open
    const char *batch_cmd = "id; pm list packages -f; settings get secure enabled_accessibility_services";
    
    if (rish_pipe_command(batch_cmd, big_buffer, 262144) == SENSEI_STATUS_OK) {
        parse_batch_output(big_buffer, results);
    }
    
    free(big_buffer);
    return SENSEI_STATUS_OK;
}
