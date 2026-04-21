#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>

#define TIMEOUT_S 5

int main(int argc, char *argv[]) {
    if (argc < 2) { fprintf(stderr, "Usage: dex_ping <socket_path>\n"); return 1; }

    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) { fprintf(stderr, "[PING] socket() failed: %s\n", strerror(errno)); return 1; }

    struct timeval tv = { .tv_sec = TIMEOUT_S, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, argv[1], sizeof(addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "[PING] connect() failed: %s\n", strerror(errno));
        close(sock); return 1;
    }

    write(sock, "PING\n", 5);

    char buf[64] = {0};
    ssize_t n = read(sock, buf, sizeof(buf) - 1);
    if (n <= 0) { fprintf(stderr, "[PING] No response (timeout or closed)\n"); close(sock); return 1; }
    buf[n] = '\0';
    if (n > 0 && buf[n-1] == '\n') buf[n-1] = '\0';
    close(sock);

    if (strcmp(buf, "OK") == 0) { printf("[PING] %s → OK\n", argv[1]); return 0; }
    if (strncmp(buf, "error:", 6) == 0) { printf("[PING] %s → %s\n", argv[1], buf); return 1; }

    fprintf(stderr, "[PING] Unexpected: '%s'\n", buf);
    return 1;
}
