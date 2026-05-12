#include "fugitoid_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

/*
 * foot_heartbeatd — Foot Clan Heartbeat Daemon
 *
 * Purpose:
 *  - Emit a simple heartbeat Splinter can monitor.
 *  - Independent of Krang and IPC Shadow.
 *  - Always-on, extremely lightweight.
 */

#define HEARTBEAT_INTERVAL_SEC 10

int main(void) {
    fugitoid_init();
    fugitoid_log("INFO", "[FOOT HEARTBEAT] foot_heartbeatd online");

    for (;;) {
        fugitoid_log("DEBUG", "[FOOT HEARTBEAT] tick");
        sleep(HEARTBEAT_INTERVAL_SEC);
    }

    return 0;
}
