#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>

#define HUB_SOCK "/data/data/com.termux/files/home/MiuiserPeruser/pipes/turtlecom.sock"

typedef struct { char cmd[16]; char label[16]; char pipe[128]; } Route;

Route routes[] = {
    {"FREQ", "THROT-TLE", "/data/data/com.termux/files/home/MiuiserPeruser/pipes/rocksteady.sock"},
    {"DISK", "DISK-O",    "/data/data/com.termux/files/home/MiuiserPeruser/pipes/metal.sock"},
    {"TEMP", "HEAT-WAVE", "/data/data/com.termux/files/home/MiuiserPeruser/pipes/leatherhead.sock"},
    {"PROCS", "ZOMBIE-FIED", "/data/data/com.termux/files/home/MiuiserPeruser/pipes/ratking.sock"},
    {"MIUI", "REPORT",    "/data/data/com.termux/files/home/MiuiserPeruser/pipes/burne.sock"}
};

int main() {
    int srv = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr = {.sun_family = AF_UNIX};
    strcpy(addr.sun_path, HUB_SOCK);
    unlink(HUB_SOCK);
    bind(srv, (struct sockaddr*)&addr, sizeof(addr));
    listen(srv, 5);

    while(1) {
        int client = accept(srv, NULL, NULL);
        char rx[64] = {0}, tx[256] = {0};
        read(client, rx, 63);
        rx[strcspn(rx, "\n\r")] = 0;

        for(int i=0; i<5; i++) {
            if(strcmp(rx, routes[i].cmd) == 0) {
                int d_fd = socket(AF_UNIX, SOCK_STREAM, 0);
                struct timeval tv = {0, 100000}; // 100ms timeout
                setsockopt(d_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
                
                struct sockaddr_un d_addr = {.sun_family = AF_UNIX};
                strcpy(d_addr.sun_path, routes[i].pipe);
                
                if(connect(d_fd, (struct sockaddr*)&d_addr, sizeof(d_addr)) == 0) {
                    char d_rx[256] = {0};
                    recv(d_fd, d_rx, 255, 0);
                    snprintf(tx, 256, "[%s] %s", routes[i].label, d_rx);
                } else {
                    snprintf(tx, 256, "[%s] OFFLINE\n", routes[i].label);
                }
                close(d_fd);
                break;
            }
        }
        send(client, tx, strlen(tx), 0);
        close(client);
    }
    return 0;
}
