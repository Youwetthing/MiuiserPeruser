/*
 * MiuiserPeruser – IPC with command handling
 */

#include "ipc.h"
#include "../core/include/leo_detection.h"
#include "../core/include/april_platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>

#define SOCKET_PATH "/data/data/com.termux/files/usr/tmp/miuiserperuser.sock"
#define CONFIG_FILE "/data/data/com.termux/files/home/.miuiserperuser_config"
#define BUFFER_SIZE 4096

static int g_listen_fd = -1;
static int g_client_fd = -1;
static pthread_t g_thread;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile bool g_running = false;
static volatile bool g_client_connected = false;

extern SENSEI_STATUS leo_full_scan(SENSEI_DETECTION_LIST *results);
extern void leo_config_reload(void);
extern void april_log(const char* level, const char* format, ...);

static void send_response(const char *msg) {
    if (g_client_fd != -1) {
        write(g_client_fd, msg, strlen(msg));
        write(g_client_fd, "\n", 1);
    }
}

static void do_full_scan(void) {
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
        send_response(buffer);
        cur = cur->next;
        count++;
    }
    snprintf(buffer, sizeof(buffer), "SUMMARY: %d detection(s) found.", count);
    send_response(buffer);
    leo_detection_list_free(&results);
}

static void do_toggle(char *module, char *state) {
    int value = (strcasecmp(state, "on") == 0 || strcasecmp(state, "1") == 0) ? 1 : 0;
    FILE *f = fopen(CONFIG_FILE, "r");
    FILE *tmp = fopen("/data/data/com.termux/files/home/.miuiserperuser_config.tmp", "w");
    if (!tmp) {
        send_response("ERROR: cannot write config");
        return;
    }
    int found = 0;
    char line[128];
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            char key[64];
            int val;
            if (sscanf(line, "%63[^=]=%d", key, &val) == 2) {
                if (strcmp(key, module) == 0) {
                    fprintf(tmp, "%s=%d\n", module, value);
                    found = 1;
                } else {
                    fprintf(tmp, "%s", line);
                }
            } else {
                fprintf(tmp, "%s", line);
            }
        }
        fclose(f);
    }
    if (!found) {
        fprintf(tmp, "%s=%d\n", module, value);
    }
    fclose(tmp);
    rename("/data/data/com.termux/files/home/.miuiserperuser_config.tmp", CONFIG_FILE);
    leo_config_reload();
    send_response("OK");
}

static void do_status(void) {
    FILE *f = fopen(CONFIG_FILE, "r");
    if (!f) {
        send_response("STATUS: using defaults (all on except kernel)");
        return;
    }
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = 0;
        send_response(line);
    }
    fclose(f);
}

static void* ipc_thread(void *arg) {
    (void)arg;
    while (g_running) {
        if (g_listen_fd == -1) {
            g_listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
            if (g_listen_fd == -1) { sleep(1); continue; }

            struct sockaddr_un addr;
            memset(&addr, 0, sizeof(addr));
            addr.sun_family = AF_UNIX;
            strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path)-1);
            unlink(SOCKET_PATH);

            if (bind(g_listen_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
                close(g_listen_fd); g_listen_fd = -1; sleep(1); continue;
            }
            if (listen(g_listen_fd, 1) == -1) {
                close(g_listen_fd); g_listen_fd = -1; sleep(1); continue;
            }
        }

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(g_listen_fd, &readfds);
        struct timeval tv = {1, 0};

        if (select(g_listen_fd+1, &readfds, NULL, NULL, &tv) <= 0)
            continue;

        if (FD_ISSET(g_listen_fd, &readfds)) {
            struct sockaddr_un client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(g_listen_fd, (struct sockaddr*)&client_addr, &client_len);
            if (client_fd != -1 && g_running) {
                pthread_mutex_lock(&g_mutex);
                g_client_fd = client_fd;
                g_client_connected = true;
                pthread_mutex_unlock(&g_mutex);

                char buffer[BUFFER_SIZE];
                ssize_t bytes = read(g_client_fd, buffer, sizeof(buffer)-1);
                if (bytes > 0) {
                    buffer[bytes] = '\0';
                    char cmd[64], arg1[64], arg2[64];
                    int n = sscanf(buffer, "%63s %63s %63s", cmd, arg1, arg2);
                    if (strcasecmp(cmd, "FULL_SCAN") == 0) {
                        do_full_scan();
                    } else if (strcasecmp(cmd, "PING") == 0) {
                        send_response("PONG");
                    } else if (strcasecmp(cmd, "TOGGLE") == 0 && n >= 3) {
                        do_toggle(arg1, arg2);
                    } else if (strcasecmp(cmd, "STATUS") == 0) {
                        do_status();
                    } else {
                        send_response("ERROR: unknown command");
                    }
                }
                close(g_client_fd);
                pthread_mutex_lock(&g_mutex);
                g_client_fd = -1;
                g_client_connected = false;
                pthread_mutex_unlock(&g_mutex);
            } else if (client_fd != -1) {
                close(client_fd);
            }
        }
    }
    return NULL;
}

SENSEI_STATUS miuiserperuser_ipc_init(void) {
    g_running = true;
    if (pthread_create(&g_thread, NULL, ipc_thread, NULL) != 0)
        return SENSEI_STATUS_ERROR;
    return SENSEI_STATUS_OK;
}

void miuiserperuser_ipc_shutdown(void) {
    g_running = false;
    if (g_client_fd != -1) close(g_client_fd);
    if (g_listen_fd != -1) close(g_listen_fd);
    unlink(SOCKET_PATH);
    pthread_join(g_thread, NULL);
    pthread_mutex_destroy(&g_mutex);
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
