#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#define HUB_PATH "/data/data/com.termux/files/home/MiuiserPeruser/pipes/turtlecom.sock"
#define KRANG_PATH "/data/data/com.termux/files/home/MiuiserPeruser/pipes/krang.sock"

int _unused_lean_main(void) {
    int hub_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un h_addr;
    
    // THE FIX: Zero out and set family explicitly
    memset(&h_addr, 0, sizeof(struct sockaddr_un));
    h_addr.sun_family = AF_UNIX;
    strncpy(h_addr.sun_path, HUB_PATH, sizeof(h_addr.sun_path) - 1);

    if (connect(hub_fd, (struct sockaddr*)&h_addr, sizeof(struct sockaddr_un)) < 0) {
        fprintf(stderr, "BRAIN: Hub Connect Failed: %s\n", strerror(errno));
        return 1;
    }
    printf("BRAIN: Connected to Sewer Hub.\n");

    char buf[1024];
    while (1) {
        ssize_t n = read(hub_fd, buf, sizeof(buf) - 1);
        if (n <= 0) break;
        buf[n] = 0;

        if (strstr(buf, "SCAN")) {
            printf("BRAIN: Routing SCAN to Krang...\n");
            int k_fd = socket(AF_UNIX, SOCK_STREAM, 0);
            struct sockaddr_un k_addr;
            
            memset(&k_addr, 0, sizeof(struct sockaddr_un));
            k_addr.sun_family = AF_UNIX;
            strncpy(k_addr.sun_path, KRANG_PATH, sizeof(k_addr.sun_path) - 1);
            
            if (connect(k_fd, (struct sockaddr*)&k_addr, sizeof(struct sockaddr_un)) == 0) {
                dprintf(k_fd, "SCAN\n");
                close(k_fd);
            } else {
                fprintf(stderr, "BRAIN: Krang Connect Failed: %s\n", strerror(errno));
            }
        }
    }
    return 0;
}
