#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>

#define KRANG_PATH "/data/data/com.termux/files/home/tmp/krang.sock"
#define BUF_SIZE 512

int main(void) {
    int server_fd, client_fd;
    char buf[BUF_SIZE];

    unlink(KRANG_PATH);

    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); exit(1); }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, KRANG_PATH);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(1);
    }

    listen(server_fd, 5);
    printf("krangd: ONLINE — listening on %s\n", KRANG_PATH);

    client_fd = accept(server_fd, NULL, NULL);
    if (client_fd < 0) { perror("accept"); exit(1); }

    printf("krangd: Splinter connected\n");

    for (;;) {
        int n = read(client_fd, buf, BUF_SIZE - 1);
        if (n > 0) {
            buf[n] = 0;
            printf("krangd: Received: %s", buf);
        }
        usleep(100000);
    }

    return 0;
}
