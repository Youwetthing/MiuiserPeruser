#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>

const char* get_sewer_pipe() {
    return "/data/data/com.termux/files/home/tmp/turtlecom.sock";
}

void sewer_prep() {
    struct stat st = {0};
    if (stat("/data/data/com.termux/files/home/tmp", &st) == -1) {
        mkdir("/data/data/com.termux/files/home/tmp", 0700);
    }
}

/* New: Every worker calls this to say "I'm here" */
void worker_announce(const char* name, const char* capability) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, get_sewer_pipe(), sizeof(addr.sun_path)-1);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
        char msg[256];
        snprintf(msg, sizeof(msg), "HELLO %s CAP=%s\n", name, capability);
        send(sock, msg, strlen(msg), 0);
    }
    close(sock);
}
