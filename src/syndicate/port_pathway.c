#include "port_pathway.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>

static int connect_to_bridge(void) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SENSEI_PORT_BRIDGE);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); /* 127.0.0.1 */

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }

    /* Set a 1 second read timeout so we never hang */
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    return sock;
}

bool probe_port_bridge(void) {
    int sock = connect_to_bridge();
    if (sock < 0) {
        return false;
    }

    const char *msg = "HELLO\n";
    (void)write(sock, msg, strlen(msg));

    char buf[16];
    ssize_t n = read(sock, buf, sizeof(buf));
    close(sock);

    /* Any response (even timeout) means: bridge exists.
       Timeout / error just means we don't trust info yet. */
    return n >= 0;
}

bool port_bridge_request_basic_info(void) {
    int sock = connect_to_bridge();
    if (sock < 0) {
        return false;
    }

    const char *msg = "INFO\n";
    if (write(sock, msg, strlen(msg)) < 0) {
        close(sock);
        return false;
    }

    char buf[256];
    ssize_t n = read(sock, buf, sizeof(buf));
    close(sock);

    if (n <= 0) {
        return false;
    }

    fwrite(buf, 1, (size_t)n, stdout);
    return true;
}
