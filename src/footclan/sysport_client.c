// sysport_client.c — Footrunner client for MiuiserPeruser System Port
// Provides READ_SYS, READ_PROC, GETPROP via localhost:8765

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SYS_PORT_ADDR "127.0.0.1"
#define SYS_PORT_PORT 8765
#define BUF_SIZE 4096

// ------------------------------------------------------------
// Connect to sysport
// ------------------------------------------------------------
static int sysport_connect(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SYS_PORT_PORT);
    inet_pton(AF_INET, SYS_PORT_ADDR, &addr.sin_addr);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

// ------------------------------------------------------------
// Send a command and read a single-line response
// ------------------------------------------------------------
static char *sysport_send_cmd(const char *cmd) {
    int fd = sysport_connect();
    if (fd < 0) return NULL;

    write(fd, cmd, strlen(cmd));
    write(fd, "\n", 1);

    char buf[BUF_SIZE];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    if (n <= 0) return NULL;

    buf[n] = '\0';

    // Expect: "OK <data>" or "ERR <reason>"
    char *out = strdup(buf);
    return out;
}

// ------------------------------------------------------------
// Public API: READ_SYS
// ------------------------------------------------------------
char *sysport_read_sys(const char *path) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "READ_SYS %s", path);
    return sysport_send_cmd(cmd);
}

// ------------------------------------------------------------
// Public API: READ_PROC
// ------------------------------------------------------------
char *sysport_read_proc(const char *path) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "READ_PROC %s", path);
    return sysport_send_cmd(cmd);
}

// ------------------------------------------------------------
// Public API: GETPROP
// ------------------------------------------------------------
char *sysport_getprop(const char *key) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "GETPROP %s", key);
    return sysport_send_cmd(cmd);
}
// ------------------------------------------------------------
// Public API: DUMPSYS
// ------------------------------------------------------------
char *sysport_dumpsys(const char *service) {
    if (!service || !*service) return NULL;

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "DUMPSYS %s", service);
    return sysport_send_cmd(cmd);
}
