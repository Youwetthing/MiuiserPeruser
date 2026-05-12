#include "fugitoid_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

/*
 * foot_resurrectord — Foot Clan Resurrection Daemon
 *
 * Purpose:
 *  - Monitor all Foot Clan daemons.
 *  - Detect when any Foot daemon dies.
 *  - Respawn the missing daemon automatically.
 *
 * Notes:
 *  - This is a simple placeholder resurrection loop.
 *  - Splinter can later override or coordinate with this.
 */

#define CHECK_INTERVAL_SEC 8

static const char *foot_daemons[] = {
    "foot_portwatchd",
    "foot_ipcshadowd",
    "foot_ipcwatchd",
    "foot_heartbeatd",
    "foot_tempbackupd",
    "foot_zombiebackupd",
};

static int daemon_running(const char *name) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "ps -A | grep -v grep | grep -q '%s'", name);
    int ret = system(cmd);
    return (ret == 0);
}

static void resurrect(const char *name) {
    fugitoid_log("WARN", "[FOOT RESURRECTOR] Attempting resurrection of %s", name);

    pid_t pid = fork();
    if (pid < 0) {
        fugitoid_log("ERROR", "[FOOT RESURRECTOR] fork() failed: %s", strerror(errno));
        return;
    }

    if (pid == 0) {
        char path[512];
        snprintf(path, sizeof(path),
                 "/data/data/com.termux/files/home/MiuiserPeruser/build/src/daemon/%s",
                 name);
        execl(path, name, NULL);
        _exit(1);
    }

    fugitoid_log("INFO", "[FOOT RESURRECTOR] %s resurrected with PID %d", name, pid);
}

int main(void) {
    fugitoid_init();
    fugitoid_log("INFO", "[FOOT RESURRECTOR] foot_resurrectord online");

    for (;;) {
        for (size_t i = 0; i < sizeof(foot_daemons)/sizeof(foot_daemons[0]); i++) {
            const char *d = foot_daemons[i];
            if (!daemon_running(d)) {
                fugitoid_log("ERROR", "[FOOT RESURRECTOR] %s is dead", d);
                resurrect(d);
            } else {
                fugitoid_log("DEBUG", "[FOOT RESURRECTOR] %s alive", d);
            }
        }

        sleep(CHECK_INTERVAL_SEC);
    }

    return 0;
}
