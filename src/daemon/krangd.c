#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#define KRANG_PATH "/data/data/com.termux/files/home/tmp/krang.sock"

int main() {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); exit(1); }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, KRANG_PATH, sizeof(addr.sun_path) - 1);

    unlink(KRANG_PATH); // Clear old socket
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind failed - check permissions");
        exit(1);
    }

    if (listen(fd, 5) < 0) { perror("listen"); exit(1); }

    printf("[KRANG] Hub ready at %s\n", KRANG_PATH);
    
    while(1) {
        int client = accept(fd, NULL, NULL);
        if (client >= 0) close(client);
        usleep(100000);
    }
    return 0;
}
