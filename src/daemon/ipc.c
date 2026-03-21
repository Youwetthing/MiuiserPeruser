#include "fugitoid_log.h"
#include "fugitoid_log.h"
/*
 * MiuiserPeruser – Master Splinter's IPC hub
 * Length‑prefixed protocol for worker communication.
 */

#include "ipc.h"
#include "../core/include/leo_detection.h"
#include "../core/include/april_platform.h"
#include "rish_pipe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <arpa/inet.h>  // for htonl, ntohl

#define DASHBOARD_SOCKET "/data/data/com.termux/files/usr/tmp/miuiserperuser.sock"
#define SEWER_SOCKET     "/data/data/com.termux/files/usr/tmp/miuiserperuser_sewer.sock"
#define BUFFER_SIZE 4096

static int g_dashboard_listen_fd = -1;
static int g_sewer_listen_fd = -1;
static int g_client_fd = -1;
static int g_worker_fd = -1;
static pthread_t g_thread;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile bool g_running = false;
static volatile bool g_client_connected = false;
static volatile bool g_worker_connected = false;

extern SENSEI_STATUS leo_full_scan(SENSEI_DETECTION_LIST *results);
extern void leo_config_reload(void);
extern char* rish_pipe_command(const char* cmd);
extern void april_log(const char* level, const char* format, ...);

/* Helper: read exactly len bytes */
static ssize_t read_full(int fd, void *buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        ssize_t n = read(fd, (char*)buf + off, len - off);
        if (n <= 0) return n;
        off += n;
    }
    return off;
}

/* Helper: write exactly len bytes */
static ssize_t write_full(int fd, const void *buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        ssize_t n = write(fd, (const char*)buf + off, len - off);
        if (n <= 0) return n;
        off += n;
    }
    return off;
}

/* Handle a worker client (single request) */
static void handle_worker_client(int client_fd) {
    uint32_t net_len;
    if (read_full(client_fd, &net_len, sizeof(net_len)) <= 0) {
        april_log("WARN", "Worker: failed to read command length");
        return;
    }
    uint32_t cmd_len = ntohl(net_len);
    if (cmd_len == 0 || cmd_len > 1024*1024) {
        april_log("WARN", "Worker: invalid command length %u", cmd_len);
        return;
    }

    char *cmd = malloc(cmd_len + 1);
    if (!cmd) return;
    if (read_full(client_fd, cmd, cmd_len) <= 0) {
        april_log("WARN", "Worker: failed to read command body");
        free(cmd);
        return;
    }
    cmd[cmd_len] = '\0';
    april_log("INFO", "Worker received command: %s", cmd);

    // For now, echo the command to test IPC.
    // Later replace with rish_pipe_command(cmd) when DEX/Shredder is ready.
    const char *response = cmd;
    uint32_t resp_len = strlen(response);
    uint32_t net_resp_len = htonl(resp_len);

    if (write_full(client_fd, &net_resp_len, sizeof(net_resp_len)) <= 0 ||
        write_full(client_fd, response, resp_len) <= 0) {
        april_log("WARN", "Worker: failed to send response");
    } else {
        april_log("INFO", "Worker: sent %u bytes", resp_len);
    }
    free(cmd);
}

static void send_response(int fd, const char *msg) {
    if (fd != -1) {
        write(fd, msg, strlen(msg));
        write(fd, "\n", 1);
    }
}

static void handle_worker_command(const char *cmd) {
    // This function is kept for compatibility but not used in new protocol.
    // Workers now use length‑prefixed messages handled by handle_worker_client.
    (void)cmd;
}

static void handle_client_command(const char *cmd) {
    if (strncmp(cmd, "FULL_SCAN", 9) == 0) {
        SENSEI_DETECTION_LIST results = {0};
        leo_full_scan(&results);
        char buffer[BUFFER_SIZE];
        SENSEI_DETECTION *cur = results.head;
        int count = 0;
        while (cur) {
            snprintf(buffer, sizeof(buffer),
                     "DETECT: [%s] %s (score %u)",
                     leo_detection_class_to_string(cur->detection_class),
                     cur->description,
                     leo_calculate_score(cur));
            send_response(g_client_fd, buffer);
            cur = cur->next;
            count++;
        }
        snprintf(buffer, sizeof(buffer), "SUMMARY: %d detection(s) found.", count);
        send_response(g_client_fd, buffer);
        leo_detection_list_free(&results);
    } else if (strncmp(cmd, "TOGGLE", 6) == 0) {
        send_response(g_client_fd, "TOGGLE not yet implemented");
    } else if (strncmp(cmd, "PING", 4) == 0) {
        send_response(g_client_fd, "PONG");
    } else {
        send_response(g_client_fd, "ERROR: unknown command");
    }
}

static int create_socket(const char *path, const char *name) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        april_log("ERROR", "Failed to create %s socket: %s", name, strerror(errno));
        return -1;
    }
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path)-1);
    unlink(path);
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        april_log("ERROR", "Failed to bind %s socket: %s", name, strerror(errno));
        close(fd);
        return -1;
    }
    if (listen(fd, 5) < 0) {
        april_log("ERROR", "Failed to listen on %s socket: %s", name, strerror(errno));
        close(fd);
        return -1;
    }
    april_log("INFO", "%s socket created, fd=%d", name, fd);
    return fd;
}

static void* ipc_thread(void *arg) {
    (void)arg;
    g_dashboard_listen_fd = create_socket(DASHBOARD_SOCKET, "Dashboard");
    g_sewer_listen_fd = create_socket(SEWER_SOCKET, "Sewer");

    april_log("INFO", "IPC thread entering main loop");
    while (g_running) {
        fd_set readfds;
        FD_ZERO(&readfds);
        int max_fd = -1;
        if (g_dashboard_listen_fd != -1) {
            FD_SET(g_dashboard_listen_fd, &readfds);
            if (g_dashboard_listen_fd > max_fd) max_fd = g_dashboard_listen_fd;
        }
        if (g_sewer_listen_fd != -1) {
            FD_SET(g_sewer_listen_fd, &readfds);
            if (g_sewer_listen_fd > max_fd) max_fd = g_sewer_listen_fd;
        }
        if (max_fd == -1) {
            sleep(1);
            continue;
        }

        struct timeval tv = {1, 0};
        int sel = select(max_fd+1, &readfds, NULL, NULL, &tv);
        if (sel < 0) {
            if (errno == EINTR) continue;
            april_log("ERROR", "select failed: %s", strerror(errno));
            sleep(1);
            continue;
        }
        if (sel == 0) continue; // timeout

        // Dashboard client
        if (g_dashboard_listen_fd != -1 && FD_ISSET(g_dashboard_listen_fd, &readfds)) {
            struct sockaddr_un client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(g_dashboard_listen_fd, (struct sockaddr*)&client_addr, &client_len);
            if (client_fd != -1 && g_running) {
                april_log("INFO", "Dashboard client connected");
                pthread_mutex_lock(&g_mutex);
                if (g_client_fd != -1) close(g_client_fd);
                g_client_fd = client_fd;
                g_client_connected = true;
                pthread_mutex_unlock(&g_mutex);

                char buffer[BUFFER_SIZE];
                ssize_t bytes = read(g_client_fd, buffer, sizeof(buffer)-1);
                if (bytes > 0) {
                    buffer[bytes] = '\0';
                    handle_client_command(buffer);
                }
                close(g_client_fd);
                pthread_mutex_lock(&g_mutex);
                g_client_fd = -1;
                g_client_connected = false;
                pthread_mutex_unlock(&g_mutex);
                april_log("INFO", "Dashboard client disconnected");
            } else if (client_fd != -1) {
                close(client_fd);
            }
        }

        // Sewer worker
        if (g_sewer_listen_fd != -1 && FD_ISSET(g_sewer_listen_fd, &readfds)) {
            struct sockaddr_un worker_addr;
            socklen_t worker_len = sizeof(worker_addr);
            int worker_fd = accept(g_sewer_listen_fd, (struct sockaddr*)&worker_addr, &worker_len);
            if (worker_fd != -1 && g_running) {
                april_log("INFO", "Worker connected via sewer, fd=%d", worker_fd);
                // Handle one request and close
                handle_worker_client(worker_fd);
                close(worker_fd);
                april_log("INFO", "Worker disconnected");
            } else if (worker_fd != -1) {
                close(worker_fd);
            }
        }
    }
    return NULL;
}

SENSEI_STATUS miuiserperuser_ipc_init(void) {
    g_running = true;
    if (pthread_create(&g_thread, NULL, ipc_thread, NULL) != 0) {
        april_log("ERROR", "Failed to create IPC thread");
        return SENSEI_STATUS_ERROR;
    }
    april_log("INFO", "IPC thread started");
    return SENSEI_STATUS_OK;
}

void miuiserperuser_ipc_shutdown(void) {
    g_running = false;
    if (g_client_fd != -1) close(g_client_fd);
    if (g_worker_fd != -1) close(g_worker_fd);
    if (g_dashboard_listen_fd != -1) close(g_dashboard_listen_fd);
    if (g_sewer_listen_fd != -1) close(g_sewer_listen_fd);
    unlink(DASHBOARD_SOCKET);
    unlink(SEWER_SOCKET);
    pthread_join(g_thread, NULL);
    pthread_mutex_destroy(&g_mutex);
    april_log("INFO", "IPC shutdown complete");
}

void miuiserperuser_ipc_broadcast(const SENSEI_DETECTION *detection) {
    pthread_mutex_lock(&g_mutex);
    if (g_client_connected && g_client_fd != -1) {
        char buffer[BUFFER_SIZE];
        snprintf(buffer, BUFFER_SIZE,
                 "ALERT: [%s] %s",
                 leo_detection_class_to_string(detection->detection_class),
                 detection->description);
        write(g_client_fd, buffer, strlen(buffer));
        write(g_client_fd, "\n", 1);
    }
    pthread_mutex_unlock(&g_mutex);
}

bool miuiserperuser_ipc_is_connected(void) {
    bool connected;
    pthread_mutex_lock(&g_mutex);
    connected = g_client_connected;
    pthread_mutex_unlock(&g_mutex);
    return connected;
}

#include <stdarg.h>

// Temporary: route april_log to stderr so IPC logs are visible
void april_log(const char* level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "[IPC][%s] ", level);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
}
