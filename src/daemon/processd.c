#include "fugitoid_log.h"
#include "fugitoid_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <ctype.h>

#include "fugitoid_log.h"
#include "krang.h"

#define PROC_PATH "/proc"

static int is_numeric(const char *s) {
    if (!s || !*s) return 0;
    while (*s) {
        if (!isdigit((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

static void scan_zombies(void) {
    DIR *dir = opendir(PROC_PATH);
    if (!dir) {
        fugitoid_log("ERROR", "[RAT KING] Failed to open %s: %s",
                    PROC_PATH, strerror(errno));
        return;
    }

    struct dirent *de;
    char stat_path[256];
    char buf[512];

    int zombie_count = 0;

    while ((de = readdir(dir)) != NULL) {
        if (!is_numeric(de->d_name))
            continue;

        snprintf(stat_path, sizeof(stat_path), "%s/%s/stat", PROC_PATH, de->d_name);
        FILE *f = fopen(stat_path, "r");
        if (!f)
            continue;

        if (!fgets(buf, sizeof(buf), f)) {
            fclose(f);
            continue;
        }
        fclose(f);

        // /proc/PID/stat: pid (comm) state ...
        // We just need the state char after the second field.
        char *p = strchr(buf, '(');
        if (!p) continue;
        char *q = strrchr(p, ')');
        if (!q) continue;

        // state is the next token after ') '
        char state = 0;
        if (sscanf(q + 2, "%c", &state) != 1)
            continue;

        if (state == 'Z') {
            zombie_count++;
            // Extract command name between '(' and ')'
            char comm[256] = {0};
            size_t len = q - p - 1;
            if (len > 0 && len < sizeof(comm)) {
                memcpy(comm, p + 1, len);
                comm[len] = '\0';
            } else {
                snprintf(comm, sizeof(comm), "pid_%s", de->d_name);
            }

            fugitoid_log("WARN",
                        "[RAT KING] Zombie process detected: PID=%s, COMM=%s",
                        de->d_name, comm);
        }
    }

    closedir(dir);

    if (zombie_count == 0) {
        fugitoid_log("INFO", "[RAT KING] No zombie processes detected this scan");
    } else {
        fugitoid_log("WARN", "[RAT KING] Total zombies this scan: %d", zombie_count);
    }
}

static void scan_cpu_hogs(void) {
    // Use Krang to run top once and capture the top CPU users.
    const char *cmd =
        "top -b -n 1 -o %CPU 2>/dev/null | head -n 15";

    char *resp = krang_send_command(cmd);
    if (!resp) {
        fugitoid_log("ERROR", "[RAT KING] Failed to run top via Krang");
        return;
    }

    fugitoid_log("INFO", "[RAT KING] Top CPU processes snapshot:\n%s", resp);
    free(resp);
}

static void ratking_heartbeat(void) {
    krang_send_command("echo processd_heartbeat");
    fugitoid_log("INFO", "[RAT KING] Heartbeat sent via Krang");
}

int main(void) {
    fugitoid_init("processd");

    fugitoid_log("INFO", "[RAT KING] processd starting up...");

    int hb_counter = 0;
    int hog_counter = 0;

    for (;;) {
        scan_zombies();

        hog_counter++;
        if (hog_counter >= 10) { // every ~10 minutes if we sleep 60s
            scan_cpu_hogs();
            hog_counter = 0;
        }

        hb_counter++;
        if (hb_counter >= 360) { // ~6 hours if we sleep 60s
            ratking_heartbeat();
            hb_counter = 0;
        }

        sleep(60);
    }

    return 0;
}
