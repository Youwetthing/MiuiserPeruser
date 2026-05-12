/*
 * krang.c — client library for sending payloads to krangd
 *
 * Connects to KRANG_SOCKET, sends a length-prefixed payload,
 * reads back the response. Caller frees the returned buffer.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>

#include "krang.h"
#include "ipc_globals.h"

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

int krang_connect(void)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        fprintf(stderr, "KRANG: socket() failed errno=%d\n", errno);
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, KRANG_SOCKET, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "KRANG: connect() failed errno=%d\n", errno);
        close(fd);
        return -1;
    }

    return fd;
}

char *krang_send_command(const char *cmd)
{
    if (!cmd) return NULL;

    int fd = krang_connect();
    if (fd < 0) return NULL;

    uint32_t cmd_len     = (uint32_t)strlen(cmd);
    uint32_t net_cmd_len = htonl(cmd_len);

    if (write_full(fd, &net_cmd_len, sizeof(net_cmd_len)) <= 0) {
        close(fd); return NULL;
    }
    if (write_full(fd, cmd, cmd_len) <= 0) {
        close(fd); return NULL;
    }

    uint32_t net_resp_len = 0;
    if (read_full(fd, &net_resp_len, sizeof(net_resp_len)) <= 0) {
        close(fd); return NULL;
    }

    uint32_t resp_len = ntohl(net_resp_len);
    char *resp = malloc((size_t)resp_len + 1);
    if (!resp) { close(fd); return NULL; }

    if (read_full(fd, resp, resp_len) <= 0) {
        free(resp); close(fd); return NULL;
    }

    resp[resp_len] = '\0';
    close(fd);
    return resp;
}
