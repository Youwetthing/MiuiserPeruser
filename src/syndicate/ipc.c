#include "ipc_includes.h"
#include "ipc_globals.h"
#include "backend_sysinfo.h"
#include "sensei_types.h"
#include "leo_detection.h"
#include "../core/log_safe.h"
#include "backend_thermals.h"
#include "backend_adb.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include "backend_doctor.h"
#include "backend_portbridge.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>

#define BUFFER_SIZE 1024

/* External detection entry points */
extern SENSEI_STATUS leo_full_scan(SENSEI_DETECTION_LIST *results);
extern void leo_config_reload(void);

/* IPC state */
static int g_dashboard_listen_fd = -1;
static int g_sewer_listen_fd     = -1;
static int g_client_fd           = -1;

static volatile bool g_client_connected = false;

/* --- helpers --- */

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
    uint32_t len = (uint32_t)strlen(msg);
    uint32_t net_len = htonl(len);

    write_full(fd, &net_len, sizeof(net_len));
write_full(fd, msg, len);
write_full(fd, "\n", 1);
}

static void handle_worker(int fd)
{
    uint32_t net_len;

    /* Read length prefix */
    if (read_full(fd, &net_len, sizeof(net_len)) <= 0)
        return;

    uint32_t len = ntohl(net_len);
    if (len == 0 || len > 1024 * 1024)
        return;

    char *cmd = malloc(len + 1);
    if (!cmd)
        return;

    /* Read command body */
    if (read_full(fd, cmd, len) <= 0) {
        free(cmd);
        return;
    }

    cmd[len] = '\0';

    /* --- COMMAND DISPATCH --- */

    /* 1. Full scan */
    if (strncmp(cmd, "FULL_SCAN", 9) == 0) {
        SENSEI_DETECTION_LIST results = {0};
        leo_full_scan(&results);
        send_response(fd, "OK");

    /* 2. Reload config */
    } else if (strncmp(cmd, "RELOAD_CONFIG", 13) == 0) {
        leo_config_reload();
        send_response(fd, "RELOADED");

    /* 3. Echo (for Splinter/Krang testing) */
    } else if (strncmp(cmd, "echo ", 5) == 0 || strncmp(cmd, "ECHO ", 5) == 0) {
        send_response(fd, cmd + 5);

    /* 4. Basic system info */
} else if (strcmp(cmd, "SYSINFO") == 0) {
    char *info = backend_sysinfo();
    send_response(fd, info ? info : "sysinfo:error");
    free(info);

/* X. Doctor mode */
} else if (strcmp(cmd, "DOCTOR") == 0) {
    char *r = backend_doctor();
    send_response(fd, r ? r : "doctor:error");
    free(r);

    /* 5. Thermals */
} else if (strcmp(cmd, "THERMALS") == 0) {
    char *t = backend_thermals();
    send_response(fd, t ? t : "thermals:error");
    free(t);

/* 5b. Thermals → Leatherhead */
} else if (strncmp(cmd, "THERMAL_REPORT ", 15) == 0) {
    /* Forward thermald’s parsed data to Leatherhead */
    const char *payload = cmd + 15;
    krang_send_command(payload);
    send_response(fd, "OK");

    /* 6. MIUI probe */
    } else if (strcmp(cmd, "MIUI_PROBE") == 0) {
        send_response(fd, "miui_probe:ok");

    /* 7. Portbridge probe */
} else if (strcmp(cmd, "PORTBRIDGE_PROBE") == 0) {
    char *r = backend_portbridge_probe();
    send_response(fd, r ? r : "portbridge:error");
    free(r);

    /* 8. RISH command */
    } else if (strncmp(cmd, "RISH ", 5) == 0) {
        send_response(fd, "rish:ok");

    /* 9. ADB command */
} else if (strncmp(cmd, "ADB ", 4) == 0) {
    const char *subcmd = cmd + 4;
    char *r = backend_adb_exec(subcmd);
    send_response(fd, r ? r : "adb:error");
    free(r);

    /* 10. Read from /proc */
    } else if (strncmp(cmd, "PROC_READ ", 10) == 0) {
        send_response(fd, "proc_read:ok");

    /* 11. Read from /sys */
    } else if (strncmp(cmd, "SYSFS_READ ", 11) == 0) {
        send_response(fd, "sysfs_read:ok");

    /* Default */
} else {
    fprintf(stderr, "[SEWER] Unknown command: '%s'\n", cmd);
    send_response(fd, "UNKNOWN");
}

    free(cmd);
}

static int create_socket(const char *path)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path)-1);

    unlink(path);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    if (listen(fd, 5) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

/* --- thread --- */

static void* ipc_thread(void *arg)
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

        if (FD_ISSET(g_dashboard_listen_fd, &fds)) {
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

        if (FD_ISSET(g_sewer_listen_fd, &fds)) {
            int wfd = accept(g_sewer_listen_fd, NULL, NULL);
            if (wfd >= 0) {
                handle_worker(wfd);
                close(wfd);
            }
        }
    }

    return NULL;
}

/* --- API --- */

SENSEI_STATUS miuiserperuser_ipc_init(void)
{
    g_dashboard_listen_fd = create_socket(DASHBOARD_SOCKET);
    g_sewer_listen_fd     = create_socket(SEWER_SOCKET);

    if (g_dashboard_listen_fd < 0 || g_sewer_listen_fd < 0)
        return SENSEI_STATUS_ERROR;

    g_running = true;

    if (pthread_create(&g_thread, NULL, ipc_thread, NULL) != 0)
        return SENSEI_STATUS_ERROR;

    return SENSEI_STATUS_OK;
}

void miuiserperuser_ipc_shutdown(void)
{
    g_running = false;

    pthread_join(g_thread, NULL);

    if (g_dashboard_listen_fd >= 0) close(g_dashboard_listen_fd);
    if (g_sewer_listen_fd >= 0) close(g_sewer_listen_fd);

    unlink(DASHBOARD_SOCKET);
    unlink(SEWER_SOCKET);
}

void miuiserperuser_ipc_broadcast(const SENSEI_DETECTION *detection)
{
    if (!detection) return;

    pthread_mutex_lock(&g_mutex);

    if (g_client_connected && g_client_fd >= 0) {
        char buffer[BUFFER_SIZE];

        snprintf(buffer, sizeof(buffer),
                 "ALERT: [%s]",
                 leo_detection_class_to_string(detection->detection_class));

        write_full(g_client_fd, buffer, strlen(buffer));
        write_full(g_client_fd, "\n", 1);
    }

    pthread_mutex_unlock(&g_mutex);
}

bool miuiserperuser_ipc_is_connected(void)
{
    return g_client_connected;
}
