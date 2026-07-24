#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>

#define HUB_PATH "/data/data/com.termux/files/home/MiuiserPeruser/pipes/turtlecom.sock"
#define PIPE_DIR "/data/data/com.termux/files/home/MiuiserPeruser/pipes"

int main() {
    mkdir(PIPE_DIR, 0700);
    unlink(HUB_PATH); // Self-heal stale socket

    int serv_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, HUB_PATH, sizeof(addr.sun_path)-1);

    if (bind(serv_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("HUB: Bind failed");
        return 1;
    }
    
    chmod(HUB_PATH, 0600);
    listen(serv_fd, 10);
    printf("TURTLECOM: Sewer Hub Online. Path: %s\n", HUB_PATH);

    char buf[1024];
    while (1) {
        int conn_fd = accept(serv_fd, NULL, NULL);
        if (conn_fd >= 0) {
            ssize_t n = read(conn_fd, buf, sizeof(buf)-1);
            if (n > 0) {
                buf[n] = 0;
                printf("HUB_RECV: %s\n", buf);
            }
            close(conn_fd);
        }
    }
    return 0;
}
