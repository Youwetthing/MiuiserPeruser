#include "fugitoid_log.h"


/*
 * bebopd – monitors wake locks and alarms (Bebop style) – DEBUG VERSION
 */

#include "krang.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define LOG_FILE "/data/data/com.termux/files/home/miuiserperuser.log"


/* Check kernel wake locks (active locks) */
static void check_wake_locks(void) {
    printf("[DEBUG] check_wake_locks started\n");
    char *resp = krang_send_command("cat /sys/power/wake_lock");
    if (!resp) {
        fugitoid_log("WARN", "krang_send_command failed for wake_lock");
        return;
    }
    printf("[DEBUG] krang_send_command returned: %s\n", resp);
    if (strlen(resp) > 0 && strcmp(resp, "ERROR") != 0 && resp[0] != '\0') {
        char buf[256];
        snprintf(buf, sizeof(buf), "Bebop sniffs active wake locks: %s", resp);
        fugitoid_log("INFO", buf);
    }
    free(resp);
    printf("[DEBUG] check_wake_locks finished\n");
}

/* Check alarm stats (top offenders) */
static void check_alarms(void) {
    printf("[DEBUG] check_alarms started\n");
    char *resp = krang_send_command("dumpsys alarm | grep -E 'package:|Alarm Stats' | head -5");
    if (!resp) {
        fugitoid_log("WARN", "krang_send_command failed for alarms");
        return;
    }
    printf("[DEBUG] check_alarms got response\n");
    if (strlen(resp) > 0) {
        fugitoid_log("INFO", "Bebop senses alarm activity – check log for details");
    }
    free(resp);
    printf("[DEBUG] check_alarms finished\n");
}

/* Check partial wake locks from batterystats */
static void check_batterystats_wakelocks(void) {
    printf("[DEBUG] check_batterystats_wakelocks started\n");
    char *resp = krang_send_command("dumpsys batterystats | grep -E 'Wake lock|Partial wake lock' | head -5");
    if (!resp) return;
    printf("[DEBUG] check_batterystats_wakelocks got response\n");
    if (strlen(resp) > 0) {
        fugitoid_log("INFO", "Bebop detects partial wake locks – possible battery drain");
    }
    free(resp);
    printf("[DEBUG] check_batterystats_wakelocks finished\n");
}

int main() {
    printf("[DEBUG] Bebop main started\n");
    fugitoid_log("INFO", "Bebop reporting for duty...");
    printf("[DEBUG] about to krang_connect\n");
    if (krang_connect() < 0) {
        fugitoid_log("ERROR", "Bebop can't reach Krang – is the sewer open?");
        printf("[DEBUG] krang_connect failed\n");
        return 1;
    }
    printf("[DEBUG] krang_connect succeeded\n");
    fugitoid_log("INFO", "Bebop connected to Krang.");

    while (1) {
        printf("[DEBUG] main loop iteration\n");
        check_wake_locks();
        check_alarms();
        check_batterystats_wakelocks();
        sleep(60);
    }

    krang_disconnect();
    return 0;
}
