#include "fugitoid_log.h"
#include "fugitoid_log.h"
/*
 * MiuiserPeruser – Matt Daemon (MIUI worker)
 * Runs as a separate process, uses Krang to talk to Master Splinter.
 */

#include "krang.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define LOG_FILE "/data/data/com.termux/files/home/miuiserperuser.log"


static void check_dual_apps(void) {
    char *resp = krang_send_command("pm list users");
    if (!resp) {
        fugitoid_log("WARN", "krang_send_command failed");
        return;
    }
    int user_count = 0;
    char *line = strtok(resp, "\n");
    while (line) {
        if (strstr(line, "UserInfo{")) user_count++;
        line = strtok(NULL, "\n");
    }
    free(resp);
    if (user_count > 1) {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "Dual Apps / Second Space active (%d users) – background overhead increased.",
                 user_count);
        fugitoid_log("INFO", buf);
    }
}

static void check_miui_optimization(void) {
    char *resp = krang_send_command("settings get global miui_optimization");
    if (!resp) return;
    int opt = atoi(resp);
    free(resp);
    if (opt == 0) {
        fugitoid_log("INFO", "MIUI optimizations are disabled – battery life may suffer.");
    }
}

static void check_game_turbo(void) {
    char *resp = krang_send_command("settings get global game_mode_enabled");
    if (!resp) return;
    int game = atoi(resp);
    free(resp);
    if (game == 1) {
        fugitoid_log("INFO", "Game Turbo is active – performance prioritized over battery.");
    }
}

int main() {
    fugitoid_log("INFO", "Matt Daemon starting...");
    if (krang_connect() < 0) {
        fugitoid_log("ERROR", "Cannot connect to Krang (master sewer). Is master running?");
        return 1;
    }
    fugitoid_log("INFO", "Connected to Krang.");

    while (1) {
        check_dual_apps();
        check_miui_optimization();
        check_game_turbo();
        sleep(60);
    }

    krang_disconnect();
    return 0;
}
