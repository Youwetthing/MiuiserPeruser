/*
 * MiuiserPeruser – MIUI-specific anomaly detection (Michelangelo / Matt Daemon)
 * Enhanced with deep Xiaomi insights using Shizuku DEX.
 */

#include <leo_detection.h>
#include <april_platform.h>
#include <sensei_types.h>
#include "../daemon/rish_pipe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

extern void april_log(const char* level, const char* format, ...);

/* Helper: run a command and return output (must be freed) */
static char* run_cmd(const char* cmd) {
    return rish_pipe_command(cmd);
}

/* Check for Dual Apps / Second Space */
static void check_dual_apps(SENSEI_DETECTION_LIST *results) {
    char *output = run_cmd("pm list users");
    if (!output) return;

    int user_count = 0;
    char *line = strtok(output, "\n");
    while (line) {
        if (strstr(line, "UserInfo")) user_count++;
        line = strtok(NULL, "\n");
    }
    free(output);

    if (user_count > 1) {
        SENSEI_DETECTION det = {0};
        det.detection_class = SENSEI_DETECTION_CLASS_BEHAVIOR;
        det.priority = SENSEI_EVENT_PRIORITY_MEDIUM;
        det.confidence = 90;
        det.mitre_id = SENSEI_MITRE_NONE;
        strncpy(det.detection_type, "DUAL_APPS_ACTIVE", SENSEI_MAX_DETECTION_TYPE-1);
        snprintf(det.description, SENSEI_MAX_DESCRIPTION-1,
                 "Dual Apps or Second Space active (%d users) – apps may not run in all spaces.", user_count);
        leo_detection_list_append(results, &det);
    }
}

/* Check MIUI optimization flag */
static void check_miui_optimization(SENSEI_DETECTION_LIST *results) {
    char *output = run_cmd("settings get global miui_optimization");
    if (!output) return;
    int opt = atoi(output);
    free(output);

    if (opt == 0) {
        SENSEI_DETECTION det = {0};
        det.detection_class = SENSEI_DETECTION_CLASS_BEHAVIOR;
        det.priority = SENSEI_EVENT_PRIORITY_MEDIUM;
        det.confidence = 80;
        det.mitre_id = SENSEI_MITRE_NONE;
        strncpy(det.detection_type, "MIUI_OPTIMIZATIONS_OFF", SENSEI_MAX_DETECTION_TYPE-1);
        snprintf(det.description, SENSEI_MAX_DESCRIPTION-1,
                 "MIUI optimizations are disabled – battery life may suffer.");
        leo_detection_list_append(results, &det);
    }
}

/* Check Game Turbo mode */
static void check_game_turbo(SENSEI_DETECTION_LIST *results) {
    char *output = run_cmd("settings get global game_mode_enabled");
    if (!output) return;
    int game = atoi(output);
    free(output);

    if (game == 1) {
        SENSEI_DETECTION det = {0};
        det.detection_class = SENSEI_DETECTION_CLASS_BEHAVIOR;
        det.priority = SENSEI_EVENT_PRIORITY_LOW;
        det.confidence = 70;
        det.mitre_id = SENSEI_MITRE_NONE;
        strncpy(det.detection_type, "GAME_TURBO_ACTIVE", SENSEI_MAX_DETECTION_TYPE-1);
        snprintf(det.description, SENSEI_MAX_DESCRIPTION-1,
                 "Game Turbo is active – performance prioritized over battery.");
        leo_detection_list_append(results, &det);
    }
}

/* Check background restrictions for critical MIUI apps */
static void check_background_restrictions(SENSEI_DETECTION_LIST *results) {
    // List of important MIUI apps that should not be restricted
    const char *critical_pkgs[] = {
        "com.miui.securitycenter",
        "com.miui.powerkeeper",
        "com.miui.daemon",
        "com.xiaomi.finddevice",
        NULL
    };
    int restricted_count = 0;
    char cmd[256];
    for (int i = 0; critical_pkgs[i]; i++) {
        snprintf(cmd, sizeof(cmd), "dumpsys package %s | grep stop_flag", critical_pkgs[i]);
        char *out = run_cmd(cmd);
        if (out) {
            // If output contains "stop_flag" with a value, it might be restricted
            // We'll just check if there's any output (indicating the flag is set)
            if (strlen(out) > 0) {
                restricted_count++;
            }
            free(out);
        }
    }
    if (restricted_count > 0) {
        SENSEI_DETECTION det = {0};
        det.detection_class = SENSEI_DETECTION_CLASS_BEHAVIOR;
        det.priority = SENSEI_EVENT_PRIORITY_MEDIUM;
        det.confidence = 85;
        det.mitre_id = SENSEI_MITRE_NONE;
        strncpy(det.detection_type, "BACKGROUND_RESTRICTED_APPS", SENSEI_MAX_DETECTION_TYPE-1);
        snprintf(det.description, SENSEI_MAX_DESCRIPTION-1,
                 "%d critical MIUI apps have background restrictions – notifications may be delayed.", restricted_count);
        leo_detection_list_append(results, &det);
    }
}

/* Main entry point – called from leo_full_scan */
SENSEI_STATUS mikey_miui(SENSEI_DETECTION_LIST *results) {
    if (!results) return SENSEI_STATUS_ERROR;

    april_log("INFO", "Running MIUI-specific checks (Matt Daemon)...");

    check_dual_apps(results);
    check_miui_optimization(results);
    check_game_turbo(results);
    check_background_restrictions(results);

    return SENSEI_STATUS_OK;
}
