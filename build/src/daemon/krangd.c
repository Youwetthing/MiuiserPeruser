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

int main(void) {
    int fd = connect_to_turtlecom();
    char buf[BUF_SIZE];
    int sent_cmd = 0;

    write(fd, "HELLO BRAIN KRANG\n", 19);
    printf("krangd: ONLINE (via Turtlecom)\n");

    for (;;) {
        if (!sent_cmd) {
            sleep(1);
            const char *cmd = "CMD START WORKER BEBOP\n";
            write(fd, cmd, strlen(cmd));
            printf("krangd: Sent: %s", cmd);
            sent_cmd = 1;
        }

        int n = read(fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = 0;
            printf("krangd: Received: %s", buf);
        }

        usleep(100000);
    }

    return 0;
}
