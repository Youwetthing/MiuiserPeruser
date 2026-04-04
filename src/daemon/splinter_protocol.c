#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "splinter.h"

#define BUS_PATH "/data/data/com.termux/files/home/tmp/turtlecom.sock"

char *krang_send_command(const char *msg) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, BUS_PATH, sizeof(addr.sun_path)-1);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        return "error:hub_offline";
    }

    dprintf(fd, "%s\n", msg);
    close(fd);
    return "ok:dispatched";
}

bool splinter_protocol_probe(void) {
    return true; 
}

bool splinter_protocol_basic_info(void) {
    return true;
}
