/*
 * MiuiserPeruser – MIUI-specific anomaly detection (Michelangelo / Matt Daemon)
 * Enhanced with deep Xiaomi insights using Shizuku DEX.
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

static char* run_cmd(const char* cmd) {
    return rish_pipe_command(cmd);
}

static int dual_apps_found = 0;
static int miui_opt_off = 0;
static int game_turbo_on = 0;
static int restricted_count = 0;

SENSEI_STATUS mikey_miui(SENSEI_DETECTION_LIST *results) {
    if (!results) return SENSEI_STATUS_ERROR;

    april_log("INFO", "Running MIUI-specific checks (Matt Daemon)...");

    // Check Dual Apps / Second Space
    char *output = run_cmd("pm list users");
    if (output) {
        int user_count = 0;
        char *line = strtok(output, "\n");
        while (line) {
            if (strstr(line, "UserInfo")) user_count++;
            line = strtok(NULL, "\n");
        }
        free(output);
        if (user_count > 1) {
            dual_apps_found = 1;
            SENSEI_DETECTION det = {0};
            det.detection_class = SENSEI_DETECTION_CLASS_BEHAVIOR;
            det.priority = SENSEI_EVENT_PRIORITY_MEDIUM;
            det.confidence = 90;
            strncpy(det.detection_type, "DUAL_APPS_ACTIVE", SENSEI_MAX_DETECTION_TYPE-1);
            snprintf(det.description, SENSEI_MAX_DESCRIPTION-1,
                     "Dual Apps or Second Space active (%d users)", user_count);
            leo_detection_list_append(results, &det);
        }
    }

    // Check MIUI optimization flag
    output = run_cmd("settings get global miui_optimization");
    if (output) {
        int opt = atoi(output);
        free(output);
        if (opt == 0) {
            miui_opt_off = 1;
            SENSEI_DETECTION det = {0};
            det.detection_class = SENSEI_DETECTION_CLASS_BEHAVIOR;
            det.priority = SENSEI_EVENT_PRIORITY_MEDIUM;
            det.confidence = 80;
            strncpy(det.detection_type, "MIUI_OPTIMIZATIONS_OFF", SENSEI_MAX_DETECTION_TYPE-1);
            snprintf(det.description, SENSEI_MAX_DESCRIPTION-1,
                     "MIUI optimizations are disabled – battery life may suffer.");
            leo_detection_list_append(results, &det);
        }
    }

    // Check Game Turbo mode
    output = run_cmd("settings get global game_mode_enabled");
    if (output) {
        int game = atoi(output);
        free(output);
        if (game == 1) {
            game_turbo_on = 1;
            SENSEI_DETECTION det = {0};
            det.detection_class = SENSEI_DETECTION_CLASS_BEHAVIOR;
            det.priority = SENSEI_EVENT_PRIORITY_LOW;
            det.confidence = 70;
            strncpy(det.detection_type, "GAME_TURBO_ACTIVE", SENSEI_MAX_DETECTION_TYPE-1);
            snprintf(det.description, SENSEI_MAX_DESCRIPTION-1,
                     "Game Turbo is active – performance prioritized over battery.");
            leo_detection_list_append(results, &det);
        }
    }

    // Check background restrictions for critical MIUI apps
    const char *critical_pkgs[] = {
        "com.miui.securitycenter",
        "com.miui.powerkeeper",
        "com.miui.daemon",
        "com.xiaomi.finddevice",
        NULL
    };
    char cmd[256];
    for (int i = 0; critical_pkgs[i]; i++) {
        snprintf(cmd, sizeof(cmd), "dumpsys package %s | grep stop_flag", critical_pkgs[i]);
        char *out = run_cmd(cmd);
        if (out && strlen(out) > 0) {
            restricted_count++;
            free(out);
        }
    }
    if (restricted_count > 0) {
        SENSEI_DETECTION det = {0};
        det.detection_class = SENSEI_DETECTION_CLASS_BEHAVIOR;
        det.priority = SENSEI_EVENT_PRIORITY_MEDIUM;
        det.confidence = 85;
        strncpy(det.detection_type, "BACKGROUND_RESTRICTED_APPS", SENSEI_MAX_DETECTION_TYPE-1);
        snprintf(det.description, SENSEI_MAX_DESCRIPTION-1,
                 "%d critical MIUI apps have background restrictions", restricted_count);
        leo_detection_list_append(results, &det);
    }

    april_log("INFO", "MIUI checks completed: dual_apps=%d, miui_opt_off=%d, game_turbo=%d, restricted=%d",
              dual_apps_found, miui_opt_off, game_turbo_on, restricted_count);

    return SENSEI_STATUS_OK;
}
