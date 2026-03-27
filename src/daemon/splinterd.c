#include "daemon_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

#define SOCK_PATH "/data/data/com.termux/files/home/tmp/turtlecom.sock"
#define KRANG_PATH "/data/data/com.termux/files/home/tmp/krang.sock"
#define BUF_SIZE 512

static int connect_sock(const char *path) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); exit(1); }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(fd);
        exit(1);
    }

    fcntl(fd, F_SETFL, O_NONBLOCK);
    return fd;
}

static void start_worker(const char *name) {
    pid_t pid = fork();
    if (pid == 0) {
        execlp(name, name, NULL);
        perror("execlp");
        _exit(1);
    }
}

int main(void) {
    if (!daemon_core_init("splinterd")) {
        return 1;
    }

    int fd_bus   = connect_sock(SOCK_PATH);
    int fd_krang = connect_sock(KRANG_PATH);

    char buf[BUF_SIZE];

    write(fd_bus, "HELLO DISPATCH SPLINTER\n", 25);
    daemon_log_info("ONLINE");

    for (;;) {
        int n = read(fd_bus, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = 0;
            daemon_log_info("Received: %s", buf);

            char cmd[32], action[32], target[64];

            if (sscanf(buf, "%31s %31s %63s", cmd, action, target) == 3 &&
                strcmp(cmd, "CMD") == 0 &&
                strcmp(action, "START") == 0) {

                daemon_log_info("starting worker '%s'", target);
                start_worker(target);
                continue;
            }

            /* Forward ALL other messages to Krang ONLY */
            write(fd_krang, buf, strlen(buf));
        }

        while (waitpid(-1, NULL, WNOHANG) > 0) {}

        usleep(100000);
    }

    daemon_core_shutdown();
    return 0;
}
