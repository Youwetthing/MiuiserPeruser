#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>

#define SOCK_PATH "/data/data/com.termux/files/home/tmp/turtlecom.sock"
#define BUF_SIZE 512

static int connect_to_turtlecom(void) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); exit(1); }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCK_PATH);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(fd);
        exit(1);
    }

    fcntl(fd, F_SETFL, O_NONBLOCK);
    return fd;
}

static void start_worker(const char *name) {
    if (fork() == 0) {
        execlp(name, name, NULL);
        perror("execlp");
        exit(1);
    }
}

int main(void) {
    int fd = connect_to_turtlecom();
    char buf[BUF_SIZE];

    write(fd, "HELLO DISPATCH SPLINTER\n", 25);
    printf("splinterd: ONLINE (via Turtlecom)\n");

    for (;;) {
        int n = read(fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = 0;
            printf("splinterd: Received: %s", buf);

            char action[32], type[32], target[64];
            if (sscanf(buf, "CMD %31s %31s %63s", action, type, target) == 3) {
                if (!strcmp(action, "START") && !strcmp(type, "WORKER")) {
                    start_worker(target);
                    char status[128];
                    snprintf(status, sizeof(status), "STATUS %s STARTED\n", target);
                    write(fd, status, strlen(status));
                }
            }
        }

        usleep(100000);
    }

    return 0;
}
