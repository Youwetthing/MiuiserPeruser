/*
 * ipc.c — MiuiserPeruser IPC layer
 *
 * Two Unix sockets:
 *   DASHBOARD_SOCKET  — persistent client (UI / monitor)
 *   SEWER_SOCKET      — one-shot worker command/response
 *
 * Splinter notifications go out via ipc_splinter_emit().
 * No sensei/leo/doctor/portbridge dependencies in this file.
 */

#include "ipc_globals.h"
#include "../core/log_safe.h"
#include "backend_sysinfo.h"
#include "backend_thermals.h"
#include "backend_adb.h"

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>

#define BUFFER_SIZE 1024

/* Forward declaration — implemented in krangd / port_pathway */
extern void krang_send_command(const char *payload);

/* ── IPC state ────────────────────────────────────────────────────────── */
static int g_dashboard_listen_fd = -1;
static int g_sewer_listen_fd     = -1;
static int g_client_fd           = -1;
static volatile bool g_client_connected = false;

/* ── Low-level I/O helpers ────────────────────────────────────────────── */

static ssize_t read_full(int fd, void *buf, size_t len)
{
    size_t off = 0;
    while (off < len) {
        ssize_t n = read(fd, (char *)buf + off, len - off);
        if (n <= 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        off += (size_t)n;
    }
    return (ssize_t)off;
}

static ssize_t write_full(int fd, const void *buf, size_t len)
{
    size_t off = 0;
    while (off < len) {
        ssize_t n = write(fd, (const char *)buf + off, len - off);
        if (n <= 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        off += (size_t)n;
    }
    return (ssize_t)off;
}

static void send_response(int fd, const char *msg)
{
    uint32_t len     = (uint32_t)strlen(msg);
    uint32_t net_len = htonl(len);
    write_full(fd, &net_len, sizeof(net_len));
    write_full(fd, msg, len);
    write_full(fd, "\n", 1);
}

/* ── Splinter emit (fire-and-forget) ──────────────────────────────────── */

void ipc_splinter_emit(const char *event)
{
    if (!event) return;

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SPLINTER_SOCKET, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
        char buf[2048];
        int len = snprintf(buf, sizeof(buf),
                           "APRIL|miuiserperuser|ipc|%s\n", event);
        if (len > 0 && len < (int)sizeof(buf))
            write(fd, buf, (size_t)len);
    }

    close(fd);
}

/* ── Command dispatch ─────────────────────────────────────────────────── */

static void handle_worker(int fd)
{
    uint32_t net_len;
    if (read_full(fd, &net_len, sizeof(net_len)) <= 0)
        return;

    uint32_t len = ntohl(net_len);
    if (len == 0 || len > 1024 * 1024)
        return;

    char *cmd = malloc(len + 1);
    if (!cmd)
        return;

    if (read_full(fd, cmd, len) <= 0) {
        free(cmd);
        return;
    }
    cmd[len] = '\0';

    /* 1. Echo */
    if (strncmp(cmd, "echo ", 5) == 0 || strncmp(cmd, "ECHO ", 5) == 0) {
        send_response(fd, cmd + 5);

    /* 2. Ping */
    } else if (strcmp(cmd, "PING") == 0) {
        send_response(fd, "PONG");

    /* 3. Sysinfo */
    } else if (strcmp(cmd, "SYSINFO") == 0) {
        char *info = backend_sysinfo();
        send_response(fd, info ? info : "sysinfo:error");
        free(info);

    /* 4. Thermals */
    } else if (strcmp(cmd, "THERMALS") == 0) {
        char *t = backend_thermals();
        send_response(fd, t ? t : "thermals:error");
        free(t);

    /* 5. Thermal report → krang forward */
    } else if (strncmp(cmd, "THERMAL_REPORT ", 15) == 0) {
        krang_send_command(cmd + 15);
        send_response(fd, "OK");

    /* 6. MIUI probe */
    } else if (strcmp(cmd, "MIUI_PROBE") == 0) {
        send_response(fd, "miui_probe:ok");

    /* 7. RISH command (stub — wire when rish_pipe is ready) */
    } else if (strncmp(cmd, "RISH ", 5) == 0) {
        send_response(fd, "rish:ok");

    /* 8. ADB command */
    } else if (strncmp(cmd, "ADB ", 4) == 0) {
        char *r = backend_adb_exec(cmd + 4);
        send_response(fd, r ? r : "adb:error");
        free(r);

    /* 9. /proc read (stub) */
    } else if (strncmp(cmd, "PROC_READ ", 10) == 0) {
        send_response(fd, "proc_read:ok");

    /* 10. /sys read (stub) */
    } else if (strncmp(cmd, "SYSFS_READ ", 11) == 0) {
        send_response(fd, "sysfs_read:ok");

    /* 11. Splinter emit passthrough */
    } else if (strncmp(cmd, "EMIT ", 5) == 0) {
        ipc_splinter_emit(cmd + 5);
        send_response(fd, "OK");

    /* Default */
    } else {
        fprintf(stderr, "[SEWER] Unknown command: '%s'\n", cmd);
        send_response(fd, "UNKNOWN");
    }

    free(cmd);
}

/* ── Socket setup ─────────────────────────────────────────────────────── */

static int create_socket(const char *path)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    unlink(path);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    if (chmod(path, 0600) < 0) {
        close(fd);
        unlink(path);
        return -1;
    }

    if (listen(fd, 5) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

/* ── IPC thread ───────────────────────────────────────────────────────── */

static void *ipc_thread(void *arg)
{
    (void)arg;

    while (g_running) {
        fd_set fds;
        FD_ZERO(&fds);
        int maxfd = -1;

        if (g_dashboard_listen_fd >= 0) {
            FD_SET(g_dashboard_listen_fd, &fds);
            if (g_dashboard_listen_fd > maxfd)
                maxfd = g_dashboard_listen_fd;
        }

        if (g_sewer_listen_fd >= 0) {
            FD_SET(g_sewer_listen_fd, &fds);
            if (g_sewer_listen_fd > maxfd)
                maxfd = g_sewer_listen_fd;
        }

        if (maxfd < 0) {
            sleep(1);
            continue;
        }

        if (select(maxfd + 1, &fds, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) continue;
            sleep(1);
            continue;
        }

        if (g_dashboard_listen_fd >= 0 &&
            FD_ISSET(g_dashboard_listen_fd, &fds)) {
            int cfd = accept(g_dashboard_listen_fd, NULL, NULL);
            if (cfd >= 0) {
                pthread_mutex_lock(&g_mutex);
                if (g_client_fd >= 0)
                    close(g_client_fd);
                g_client_fd = cfd;
                g_client_connected = true;
                pthread_mutex_unlock(&g_mutex);
            }
        }

        if (g_sewer_listen_fd >= 0 &&
            FD_ISSET(g_sewer_listen_fd, &fds)) {
            int wfd = accept(g_sewer_listen_fd, NULL, NULL);
            if (wfd >= 0) {
                handle_worker(wfd);
                close(wfd);
            }
        }
    }

    return NULL;
}

/* ── Public API ───────────────────────────────────────────────────────── */

int miuiserperuser_ipc_init(void)
{
    g_dashboard_listen_fd = create_socket(DASHBOARD_SOCKET);
    g_sewer_listen_fd     = create_socket(SEWER_SOCKET);

    if (g_dashboard_listen_fd < 0 || g_sewer_listen_fd < 0)
        return -1;

    g_running = true;

    if (pthread_create(&g_thread, NULL, ipc_thread, NULL) != 0)
        return -1;

    return 0;
}

void miuiserperuser_ipc_shutdown(void)
{
    g_running = false;
    pthread_join(g_thread, NULL);

    if (g_dashboard_listen_fd >= 0) close(g_dashboard_listen_fd);
    if (g_sewer_listen_fd     >= 0) close(g_sewer_listen_fd);
    if (g_client_fd           >= 0) close(g_client_fd);

    unlink(DASHBOARD_SOCKET);
    unlink(SEWER_SOCKET);
}

bool miuiserperuser_ipc_is_connected(void)
{
    return g_client_connected;
}
