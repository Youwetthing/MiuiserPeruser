#include "daemon_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>

#define SOCK_PATH "/data/data/com.termux/files/home/tmp/turtlecom.sock"
#define BUF_SIZE 512

static int create_server_socket(void) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); exit(1); }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCK_PATH);

    unlink(SOCK_PATH);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        exit(1);
    }

    if (listen(fd, 5) < 0) {
        perror("listen");
        close(fd);
        exit(1);
    }

    fcntl(fd, F_SETFL, O_NONBLOCK);
    return fd;
}

int main(void) {
    if (!daemon_core_init("turtlecomd")) return 1;

    int server_fd = create_server_socket();
    daemon_log_info("ONLINE");

    int clients[16] = {0};

    for (;;) {
        int cfd = accept(server_fd, NULL, NULL);
        if (cfd >= 0) {
            fcntl(cfd, F_SETFL, O_NONBLOCK);
            for (int i = 0; i < 16; i++) {
                if (clients[i] == 0) {
                    clients[i] = cfd;
                    daemon_log_info("client connected");
                    break;
                }
            }
        }

        char buf[BUF_SIZE];

        for (int i = 0; i < 16; i++) {
            if (clients[i] == 0) continue;

            int n = read(clients[i], buf, sizeof(buf) - 1);
            if (n > 0) {
                buf[n] = 0;
                daemon_log_info("Received: %s", buf);

                for (int j = 0; j < 16; j++) {
                    if (clients[j] != 0 && j != i) {
                        write(clients[j], buf, strlen(buf));
                    }
                }
            }
        }

        usleep(100000);
    }

    daemon_core_shutdown();
    return 0;
}

