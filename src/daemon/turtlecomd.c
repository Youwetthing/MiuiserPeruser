#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <errno.h>

#define SOCK_PATH "/data/data/com.termux/files/home/tmp/turtlecom.sock"

static int create_server_socket(void) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); exit(1); }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCK_PATH, sizeof(addr.sun_path) - 1);

    unlink(SOCK_PATH);
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(1);
    }

    listen(fd, 5);
    return fd;
}

int main() {
    signal(SIGPIPE, SIG_IGN); // Prevent "Broken Pipe" crashes
    int server_fd = create_server_socket();
    printf("[INFO] turtlecomd: ONLINE\n");

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) continue;
        
        char buf[256] = {0};
        read(client_fd, buf, sizeof(buf)-1);
        printf("[INFO] turtlecomd: Received: %s\n", buf);
        close(client_fd);
    }
    return 0;
}
