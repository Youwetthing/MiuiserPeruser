#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "/data/data/com.termux/files/usr/tmp/miuiserperuser.sock"
#define BUFFER_SIZE 4096

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <command> [args...]\n", argv[0]);
        fprintf(stderr, "Commands:\n");
        fprintf(stderr, "  FULL_SCAN              Run a full scan\n");
        fprintf(stderr, "  PING                   Test connection\n");
        fprintf(stderr, "  STATUS                 Show module states\n");
        fprintf(stderr, "  TOGGLE <module> on|off Enable/disable a module\n");
        return 1;
    }

    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path)-1);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }

    char cmd[256] = "";
    for (int i = 1; i < argc; i++) {
        if (i > 1) strcat(cmd, " ");
        strcat(cmd, argv[i]);
    }
    write(sock, cmd, strlen(cmd));
    shutdown(sock, SHUT_WR);

    char buffer[BUFFER_SIZE];
    ssize_t bytes;
    while ((bytes = read(sock, buffer, sizeof(buffer)-1)) > 0) {
        buffer[bytes] = '\0';
        printf("%s", buffer);
    }
    close(sock);
    return 0;
}
