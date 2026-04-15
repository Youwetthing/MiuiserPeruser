/*
 * MiuiserPeruser – Network monitor (Raphael)
 * DEBUG VERSION – Logs on every call and shows raw data preview.
 */

#include <leo_detection.h>
#include <april_platform.h>
#include <sensei_types.h>
#include "rish_pipe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

extern void april_log(const char* level, const char* format, ...);

SENSEI_STATUS raph_network_scan(SENSEI_DETECTION_LIST *results) {
    (void)results;

    char *output = rish_pipe_command("dumpsys netstats detail");
    if (!output) {
        april_log("WARN", "Network scan: rish_pipe_command returned NULL");
        return SENSEI_STATUS_OK;
    }

    if (strlen(output) == 0) {
        april_log("WARN", "Network scan: rish_pipe_command returned empty string");
        free(output);
        return SENSEI_STATUS_OK;
    }

    // Print a preview of the raw data (first 80 chars) to prove we got it
    char preview[81];
    strncpy(preview, output, 80);
    preview[80] = '\0';
    // Remove newlines for clean logging
    for (int i = 0; preview[i]; i++) if (preview[i] == '\n') preview[i] = ' ';
    april_log("INFO", "Network raw data preview: %s", preview);

    // Count active UIDs (as before)
    int active_uids = 0;
    char *line = strtok(output, "\n");
    while (line) {
        if (strstr(line, "uid=")) {
            char *uid_str = strstr(line, "uid=");
            if (uid_str) {
                int uid = atoi(uid_str + 4);
                if (uid > 10000) active_uids++;
            }
        }
        line = strtok(NULL, "\n");
    }

    april_log("INFO", "Network scan completed: %d apps have network activity", active_uids);
    free(output);
    return SENSEI_STATUS_OK;
}
