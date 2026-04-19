#include "fugitoid_log.h"
#include "krang.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/*
 * foot_ipcshadowd — Foot Clan IPC Shadow Daemon
 *
 * Purpose:
 *  - Provide a fallback IPC channel when Krang is dead or blocked.
 *  - Accept minimal commands.
 *  - Respond with simple text.
 *  - Stay alive indefinitely.
 */

#define SHADOW_PORT 42424
#define BUF_SIZE 512

static int shadow_listen_socket(void) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        fugitoid_log("ERROR", "[FOOT IPC] socket() failed: %s", strerror(errno));
        return -1;
    }

    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(SHADOW_PORT),
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK)
    };

    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fugitoid_log("ERROR", "[FOOT IPC] bind() failed: %s", strerror(errno));
        close(s);
        return -1;
    }

    if (listen(s, 4) < 0) {
        fugitoid_log("ERROR", "[FOOT IPC] listen() failed: %s", strerror(errno));
        close(s);
        return -1;
    }

    fugitoid_log("INFO", "[FOOT IPC] Shadow IPC listening on port %d", SHADOW_PORT);
    return s;
}

static void handle_client(int cfd) {
    char buf[BUF_SIZE] = {0};
    ssize_t n = read(cfd, buf, sizeof(buf)-1);

    if (n <= 0) {
        close(cfd);
        return;
    }

    buf[n] = '\0';
    fugitoid_log("DEBUG", "[FOOT IPC] Received: %s", buf);

    if (strncmp(buf, "ping", 4) == 0) {
        write(cfd, "pong\n", 5);
    } else if (strncmp(buf, "status", 6) == 0) {
        write(cfd, "shadow_ok\n", 10);
    } else {
        write(cfd, "unknown\n", 8);
    }

    close(cfd);
}

int main(void) {
    fugitoid_init();
    fugitoid_log("INFO", "[FOOT IPC] foot_ipcshadowd online");

    int sfd = shadow_listen_socket();
    if (sfd < 0) {
        fugitoid_log("ERROR", "[FOOT IPC] Failed to start shadow IPC");
        return 1;
    }

    for (;;) {
        int cfd = accept(sfd, NULL, NULL);
        if (cfd < 0) {
            fugitoid_log("WARN", "[FOOT IPC] accept() failed: %s", strerror(errno));
            sleep(1);
            continue;
        }

        handle_client(cfd);
    }

    return 0;
}
