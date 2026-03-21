#include "fugitoid_log.h"
#include "fugitoid_log.h"
#include "dex_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SHREDDER_SOCKET "/data/data/com.termux/files/usr/tmp/miuiserperuser_shredder.sock"
#define BUFFER_SIZE 16384

char* dex_command(const char* cmd) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) return strdup("ERROR: socket failed");
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SHREDDER_SOCKET, sizeof(addr.sun_path)-1);
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return strdup("ERROR: cannot connect to Shredder");
    }
    dprintf(sock, "%s\n", cmd);
    shutdown(sock, SHUT_WR);
    char *resp = malloc(BUFFER_SIZE);
    if (!resp) {
        close(sock);
        return strdup("ERROR: malloc failed");
    }
    int pos = 0;
    while (pos < BUFFER_SIZE-1) {
        ssize_t n = read(sock, resp + pos, BUFFER_SIZE-1-pos);
        if (n <= 0) break;
        pos += n;
    }
    resp[pos] = '\0';
    close(sock);
    return resp;
}
