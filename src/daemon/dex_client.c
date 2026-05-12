#include "../core/log_safe.h"
#include "dex_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

char* dex_command(const char* cmd) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        log_event(LOG_LEVEL_ERROR, "DEX", "socket_failed errno=%d", errno);
        return strdup("ERROR: socket failed");
    }

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, DEX_SOCKET_PATH, sizeof(addr.sun_path)-1);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        log_event(LOG_LEVEL_ERROR, "DEX", "connect_failed errno=%d", errno);
        return strdup("ERROR: cannot connect to Shredder");
    }

    log_event(LOG_LEVEL_DEBUG, "DEX", "sending_command");
    dprintf(sock, "%s\n", cmd);  // keep logic identical

    char *resp = malloc(BUFFER_SIZE);
    if (!resp) {
        close(sock);
        log_event(LOG_LEVEL_ERROR, "DEX", "malloc_failed");
        return strdup("ERROR: malloc failed");
    }

    size_t pos = 0;
    while (pos < BUFFER_SIZE-1) {
        ssize_t n = read(sock, resp + pos, BUFFER_SIZE-1-pos);
        if (n <= 0) break;
        pos += n;
    }

    close(sock);
    log_event(LOG_LEVEL_DEBUG, "DEX", "response_received bytes=%d", pos);
    return resp;
}
