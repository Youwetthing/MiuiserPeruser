#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>

#define SOCK_PATH "/data/data/com.termux/files/home/tmp/turtlecom.sock"

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

int main(void) {
    int fd = connect_to_turtlecom();
    write(fd, "HELLO WORKER ROCKSTEADY\n", 25);
    printf("rocksteadyd: ONLINE\n");

    for (;;) {
        const char *msg = "STATUS ROCKSTEADY OK\n";
        write(fd, msg, strlen(msg));
        usleep(500000);
    }

    return 0;
}
