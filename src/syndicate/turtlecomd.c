#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>

#include "syndicate_core.h"
#include "../core/april_table.h"
#include "../core/ipc_utils.h"

#define BASE_DIR    "/data/data/com.termux/files/home/MiuiserPeruser"
#define PIPES_DIR   BASE_DIR "/pipes"
#define KRANG_SOCK  PIPES_DIR "/krang.sock"
#define SOCK_PATH   PIPES_DIR "/turtlecom.sock"
#define LOG_PREFIX  "turtlecomd"

#define KRANG_POLL_INTERVAL_US  500000
#define KRANG_POLL_TIMEOUT_S    30
#define FORWARD_TIMEOUT_S       5

static int server_fd = -1;

static void cleanup(int sig) {
    if (server_fd >= 0) close(server_fd);
    unlink(SOCK_PATH);
    printf("[TURTLECOMD] Shutdown complete.\n");
    exit(0);
}

static int wait_for_krang(void) {
    int attempts = (KRANG_POLL_TIMEOUT_S * 1000000) / KRANG_POLL_INTERVAL_US;
    printf("[TURTLECOMD] Waiting for krang.sock");
    fflush(stdout);
    for (int i = 0; i < attempts; i++) {
        if (access(KRANG_SOCK, F_OK) == 0) { printf(" ready.\n"); return 0; }
        printf("."); fflush(stdout);
        usleep(KRANG_POLL_INTERVAL_US);
    }
    printf("\n"); return -1;
}

static char *forward_to_krang(const char *msg) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) return strdup("error:socket");

    struct timeval tv = { .tv_sec = FORWARD_TIMEOUT_S, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, KRANG_SOCK, sizeof(addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock); return strdup("error:krang_down");
    }

    write_line(sock, msg);

    char *resp = malloc(1024);
    if (!resp) { close(sock); return strdup("error:oom"); }

    ssize_t n = read_line(sock, resp, 1023);
    if (n <= 0) { free(resp); close(sock); return strdup("error:no_response"); }

    close(sock);
    return resp;
}

int main(void) {
    signal(SIGINT,  cleanup);
    signal(SIGTERM, cleanup);
    syndicate_init();

    if (access(PIPES_DIR, F_OK) != 0) {
        fprintf(stderr, "[TURTLECOMD] FATAL: pipes/ missing. Run install.sh\n"); return 1;
    }
    if (wait_for_krang() != 0) {
        fprintf(stderr, "[TURTLECOMD] FATAL: krang.sock not available after %ds\n",
                KRANG_POLL_TIMEOUT_S); return 1;
    }

    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        fprintf(stderr, "[TURTLECOMD] FATAL: socket() failed: %s\n", strerror(errno)); return 1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCK_PATH, sizeof(addr.sun_path) - 1);
    unlink(SOCK_PATH);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "[TURTLECOMD] FATAL: bind() failed: %s\n", strerror(errno)); return 1;
    }
    if (listen(server_fd, 10) < 0) {
        fprintf(stderr, "[TURTLECOMD] FATAL: listen() failed: %s\n", strerror(errno)); return 1;
    }

    printf("[TURTLECOMD] ONLINE — gateway active (turtlecom → krang)\n");

    while (1) {
        int client = accept(server_fd, NULL, NULL);
        if (client < 0) continue;

        uint32_t log_level = april_read(APRIL_LOG_LEVEL,  LOG_NORMAL);
        uint32_t lock      = april_read(APRIL_SYSTEM_LOCK, SYSLOCK_NORMAL);
        uint32_t route     = april_read(APRIL_ROUTE_MODE,  ROUTE_AUTO);

        if (lock == SYSLOCK_LOCKED) {
            if (log_level >= LOG_NORMAL)
                log_cabin(LOG_PREFIX, "decision: system_locked — request rejected");
            write_line(client, "error:system_locked");
            close(client); continue;
        }

        char buf[1024] = {0};
        ssize_t n = read_line(client, buf, sizeof(buf));
        if (n <= 0) { close(client); continue; }

        if (log_level >= LOG_NORMAL)  log_cabin(LOG_PREFIX, buf);
        if (log_level == LOG_VERBOSE) printf("[TURTLECOMD] ROUTE: %s", buf);

        char *resp = NULL;
        if (route == ROUTE_TCP_ONLY) {
            if (log_level >= LOG_NORMAL)
                log_cabin(LOG_PREFIX, "decision: tcp_only_mode");
            resp = strdup("error:tcp_only_mode");
        } else {
            if (log_level >= LOG_NORMAL)
                log_cabin(LOG_PREFIX, "decision: forward_to_krang");
            resp = forward_to_krang(buf);
        }

        write_line(client, resp);
        free(resp);
        close(client);
    }
}
