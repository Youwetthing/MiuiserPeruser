#include "ipc.h"
#include "ipc_globals.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>

// Global FDs from ipc_globals.c
extern int g_dashboard_listen_fd;

SENSEI_STATUS miuiserperuser_ipc_init(void) {
    printf("[IPC] Initializing Turtle Power Sewer Link...\n");
    
    struct sockaddr_un addr;
    g_dashboard_listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_dashboard_listen_fd == -1) {
        perror("[IPC] socket error");
        return SENSEI_STATUS_ERROR;
    }

    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, "pipes/turtlecom.sock", sizeof(addr.sun_path) - 1);

    unlink(addr.sun_path); // Clear old ghosts

    if (bind(g_dashboard_listen_fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) == -1) {
        perror("[IPC] bind error");
        close(g_dashboard_listen_fd);
        return SENSEI_STATUS_ERROR;
    }

    if (listen(g_dashboard_listen_fd, 5) == -1) {
        perror("[IPC] listen error");
        close(g_dashboard_listen_fd);
        return SENSEI_STATUS_ERROR;
    }

    printf("[IPC] Sewer Pipe Open at %s\n", addr.sun_path);
    return SENSEI_STATUS_OK;
}

void miuiserperuser_ipc_shutdown(void) {
    printf("[IPC] Closing Sewer Pipes...\n");
    if (g_dashboard_listen_fd != -1) {
        close(g_dashboard_listen_fd);
        g_dashboard_listen_fd = -1;
    }
    unlink("pipes/turtlecom.sock");
}

bool miuiserperuser_ipc_is_connected(void) {
    return g_dashboard_listen_fd != -1;
}

void miuiserperuser_ipc_broadcast(const SENSEI_DETECTION *detection) {
    // Stub for now so the Mouser doesn't choke
    if (!detection) return;
}
