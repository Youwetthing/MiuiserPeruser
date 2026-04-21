#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "syndicate_core.h"
#include "../core/april_table.h"
#include "../core/ipc_utils.h"

#define BASE_DIR  "/data/data/com.termux/files/home/MiuiserPeruser"
#define PIPES_DIR BASE_DIR "/pipes"
#define SOCK_PATH PIPES_DIR "/krang.sock"
#define LOG_DIR   BASE_DIR "/Log_Cabin"
#define LOG_PREFIX "krangd"

static int server_fd = -1;

static void cleanup(int sig) {
    if (server_fd >= 0) close(server_fd);
    unlink(SOCK_PATH);
    printf("[KRANGD] Shutdown complete.\n");
    exit(0);
}

int main(void) {
    signal(SIGINT,  cleanup);
    signal(SIGTERM, cleanup);
    syndicate_init();

    if (access(PIPES_DIR, F_OK) != 0) {
        fprintf(stderr, "[KRANGD] FATAL: pipes/ missing. Run install.sh\n"); return 1;
    }
    if (access(LOG_DIR, F_OK) != 0) {
        fprintf(stderr, "[KRANGD] FATAL: Log_Cabin/ missing. Run install.sh\n"); return 1;
    }

    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        fprintf(stderr, "[KRANGD] FATAL: socket() failed: %s\n", strerror(errno)); return 1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCK_PATH, sizeof(addr.sun_path) - 1);
    unlink(SOCK_PATH);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "[KRANGD] FATAL: bind() failed: %s\n", strerror(errno)); return 1;
    }
    if (listen(server_fd, 10) < 0) {
        fprintf(stderr, "[KRANGD] FATAL: listen() failed: %s\n", strerror(errno)); return 1;
    }

    printf("[KRANGD] ONLINE — core processor active\n");

    while (1) {
        int client = accept(server_fd, NULL, NULL);
        if (client < 0) continue;

        if (april_read(APRIL_SYSTEM_LOCK, SYSLOCK_NORMAL) == SYSLOCK_LOCKED) {
            write_line(client, "error:system_locked");
            close(client); continue;
        }

        char buf[1024] = {0};
        ssize_t n = read_line(client, buf, sizeof(buf));
        if (n <= 0) { close(client); continue; }

        uint32_t log_level  = april_read(APRIL_LOG_LEVEL,  LOG_NORMAL);
        uint32_t krang_mode = april_read(APRIL_KRANG_MODE, KRANG_ACTIVE);

        if (log_level >= LOG_NORMAL)  log_cabin(LOG_PREFIX, buf);
        if (log_level >= LOG_NORMAL)  db_log(LOG_PREFIX, "IPC", buf);
        if (log_level == LOG_VERBOSE) printf("[KRANGD] CMD: %s", buf);

        if (krang_mode == KRANG_PASS_THROUGH) {
            if (log_level >= LOG_NORMAL)
                log_cabin(LOG_PREFIX, "mode: pass_through");
            write_line(client, "OK:pass_through");
        } else if (krang_mode == KRANG_OFFLINE) {
            if (log_level >= LOG_NORMAL)
                log_cabin(LOG_PREFIX, "mode: offline — rejected");
            write_line(client, "error:krang_offline");
        } else {
            write_line(client, "OK");
        }
        close(client);
    }
}
