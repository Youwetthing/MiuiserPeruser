// metalhead.c — MIUI specialist daemon
// Backend TMNT daemon: Metalhead
// Dashboard persona: Matt Daemon

#include "krang.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void send_status(const char *key, const char *value) {
    char buf[256];
    snprintf(buf, sizeof(buf), "%s=%s", key, value);
    krang_send_command(buf);
}

static char* run(const char *cmd) {
    return krang_send_command(cmd); // returns malloc'd string
}

static void check_miui_optimization(void) {
    char *resp = run("settings get global miui_optimization");
    if (!resp) return;

    if (strstr(resp, "0"))
        send_status("MIUI_OPTIMIZATION", "disabled");
    else
        send_status("MIUI_OPTIMIZATION", "enabled");
    free(resp);
}

static void check_power_optimization(void) {
    char *resp = run("settings get secure power_optimization");
    if (!resp) return;

    if (strstr(resp, "1"))
        send_status("POWER_OPTIMIZATION", "enabled");
    else
        send_status("POWER_OPTIMIZATION", "disabled");
    free(resp);
}

static void check_autostart(void) {
    char *resp = run("settings get secure app_auto_start");
    if (!resp) return;

    if (strstr(resp, "1"))
        send_status("AUTOSTART", "allowed");
    else
        send_status("AUTOSTART", "blocked");
    free(resp);
}

static void check_hidden_api_policy(void) {
    char *resp = run("settings get global hidden_api_policy");
    if (!resp) return;

    if (strstr(resp, "1"))
        send_status("HIDDEN_API_POLICY", "relaxed");
    else if (strstr(resp, "2"))
        send_status("HIDDEN_API_POLICY", "whitelist");
    else
        send_status("HIDDEN_API_POLICY", "strict");
    free(resp);
}

static void check_dual_apps(void) {
    char *resp = run("pm list users");
    if (!resp) return;

    if (strstr(resp, "DualSpace"))
        send_status("DUAL_APPS", "enabled");
    else
        send_status("DUAL_APPS", "disabled");
    free(resp);
}

static void check_game_turbo(void) {
    char *resp = run("getprop persist.sys.miui.gamemode");
    if (!resp) return;

    if (strstr(resp, "1"))
        send_status("GAME_TURBO", "active");
    else
        send_status("GAME_TURBO", "inactive");
    free(resp);
}

static void check_bg_restrictions(void) {
    char *resp = run("dumpsys deviceidle");
    if (!resp) return;

    if (strstr(resp, "mState=ACTIVE"))
        send_status("BG_RESTRICTION", "normal");
    else
        send_status("BG_RESTRICTION", "aggressive");
    free(resp);
}

static void run_all_checks(void) {
    check_miui_optimization();
    check_power_optimization();
    check_autostart();
    check_hidden_api_policy();
    check_dual_apps();
    check_game_turbo();
    check_bg_restrictions();
}

int main(void) {
    while (krang_connect() != 0) {
        sleep(2);
    }
    krang_send_command("METALHEAD=online");

    for (;;) {
        run_all_checks();
        sleep(60);
    }

    return 0;
}
