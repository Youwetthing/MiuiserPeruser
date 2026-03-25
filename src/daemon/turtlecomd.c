#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>

#define SOCK_PATH "/data/data/com.termux/files/home/tmp/turtlecom.sock"
#define MAX_CLIENTS 16
#define BUF_SIZE 512

int main(void) {
    int server_fd, client_fd, max_fd, i;
    int clients[MAX_CLIENTS] = {0};
    char buf[BUF_SIZE];

    unlink(SOCK_PATH);

    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCK_PATH);

    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 5);

    printf("turtlecomd: ONLINE — socket at %s\n", SOCK_PATH);

    for (;;) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        max_fd = server_fd;

        for (i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i] > 0) {
                FD_SET(clients[i], &readfds);
                if (clients[i] > max_fd)
                    max_fd = clients[i];
            }
        }

        select(max_fd + 1, &readfds, NULL, NULL, NULL);

        if (FD_ISSET(server_fd, &readfds)) {
            client_fd = accept(server_fd, NULL, NULL);
            for (i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i] == 0) {
                    clients[i] = client_fd;
                    write(client_fd, "HELLO\n", 6);
                    break;
                }
            }
        }

        for (i = 0; i < MAX_CLIENTS; i++) {
            int fd = clients[i];
            if (fd > 0 && FD_ISSET(fd, &readfds)) {
                int n = read(fd, buf, BUF_SIZE - 1);
                if (n <= 0) {
                    close(fd);
                    clients[i] = 0;
                    continue;
                }
                buf[n] = 0;
                printf("turtlecomd: received: %s", buf);

                for (int j = 0; j < MAX_CLIENTS; j++) {
                    if (clients[j] > 0 && j != i) {
                        write(clients[j], buf, strlen(buf));
                    }
                }
            }
        }
    }

    return 0;
}
