#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define BUFFER_SIZE 4096
#define DEX_SOCKET_PATH "/data/data/com.termux/files/home/MiuiserPeruser/pipes/dex.sock"

char *dex_query(const char *msg) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) return NULL;

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, DEX_SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return NULL;
    }

    write(sock, msg, strlen(msg));

    char *resp = malloc(BUFFER_SIZE);
    if (!resp) {
        close(sock);
        return NULL;
    }

    int pos = 0;
    while (pos < BUFFER_SIZE - 1) {
        ssize_t n = read(sock, resp + pos, BUFFER_SIZE - 1 - pos);
        if (n <= 0) break;
        pos += n;
    }

    resp[pos] = '\0';
    close(sock);
    return resp;
}
