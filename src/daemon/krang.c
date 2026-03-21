#include "fugitoid_log.h"
#include "fugitoid_log.h"
#include "krang.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>

#define SEWER_SOCKET "/data/data/com.termux/files/usr/tmp/miuiserperuser_sewer.sock"

static ssize_t read_full(int fd, void *buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        ssize_t n = read(fd, (char*)buf + off, len - off);
        if (n <= 0) return n;
        off += n;
    }
    return off;
}

static ssize_t write_full(int fd, const void *buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        ssize_t n = write(fd, (const char*)buf + off, len - off);
        if (n <= 0) return n;
        off += n;
    }
    return off;
}

int krang_connect(void) {
    return 0; // one-shot mode
}

char* krang_send_command(const char* cmd) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return strdup("ERROR: socket failed");

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SEWER_SOCKET, sizeof(addr.sun_path)-1);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return strdup("ERROR: connect failed");
    }

    uint32_t cmd_len = strlen(cmd);
    uint32_t net_cmd_len = htonl(cmd_len);

    fprintf(stderr, "[krang] sending '%s' (%u bytes)\n", cmd, cmd_len);
    if (write_full(fd, &net_cmd_len, sizeof(net_cmd_len)) <= 0 ||
        write_full(fd, cmd, cmd_len) <= 0) {
        close(fd);
        return strdup("ERROR: write failed");
    }

    uint32_t net_resp_len;
    if (read_full(fd, &net_resp_len, sizeof(net_resp_len)) <= 0) {
        close(fd);
        return strdup("ERROR: read length failed");
    }
    uint32_t resp_len = ntohl(net_resp_len);
    if (resp_len == 0 || resp_len > 1024*1024) {
        close(fd);
        return strdup("ERROR: invalid response length");
    }

    char *resp = malloc(resp_len + 1);
    if (!resp) {
        close(fd);
        return strdup("ERROR: malloc failed");
    }
    if (read_full(fd, resp, resp_len) <= 0) {
        free(resp);
        close(fd);
        return strdup("ERROR: read response failed");
    }
    resp[resp_len] = '\0';
    close(fd);
    fprintf(stderr, "[krang] received %u bytes\n", resp_len);
    return resp;
}

void krang_disconnect(void) {
    // no-op
}
