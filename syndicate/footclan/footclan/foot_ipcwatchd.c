#include "fugitoid_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>

/*
 * foot_ipcwatchd — Foot Clan IPC Watchdog Daemon
 *
 * Purpose:
 *  - Monitor foot_ipcshadowd on port 56789.
 *  - Detect if the shadow IPC daemon is dead or unresponsive.
 *  - Emit logs so Splinter can react.
 */

#define SHADOW_PORT 56789
#define WATCH_INTERVAL_SEC 10

static int shadow_alive(void) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return 0;

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(SHADOW_PORT),
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK)
    };

    int ok = (connect(s, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    close(s);
    return ok;
}

int main(void) {
    fugitoid_init();
    fugitoid_log("INFO", "[FOOT IPCWATCH] foot_ipcwatchd online");

    for (;;) {
        if (!shadow_alive()) {
            fugitoid_log("ERROR", "[FOOT IPCWATCH] foot_ipcshadowd appears dead");
        } else {
            fugitoid_log("DEBUG", "[FOOT IPCWATCH] shadow IPC alive");
        }

        sleep(WATCH_INTERVAL_SEC);
    }

    return 0;
}
