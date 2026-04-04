#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <errno.h>

#define HUB_PATH "/data/data/com.termux/files/home/MiuiserPeruser/pipes/turtlecom.sock"
#define KRANG_PATH "/data/data/com.termux/files/home/MiuiserPeruser/pipes/krang.sock"

// Simple "Fire-and-Forget" Dispatch
void dispatch_to_worker(const char* worker_path, const char* command) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, worker_path, sizeof(addr.sun_path)-1);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
        write(sock, command, strlen(command));
    }
    close(sock); // Instant release - No waiting!
}

int main() {
    int hub_fd = -1;
    struct sockaddr_un h_addr;
    memset(&h_addr, 0, sizeof(h_addr));
    h_addr.sun_family = AF_UNIX;
    strncpy(h_addr.sun_path, HUB_PATH, sizeof(h_addr.sun_path)-1);

    printf("BRAIN: Initializing Async Router...\n");

    while (1) {
        // 1. Connection Logic (Retry Loop)
        if (hub_fd < 0) {
            hub_fd = socket(AF_UNIX, SOCK_STREAM, 0);
            if (connect(hub_fd, (struct sockaddr*)&h_addr, sizeof(h_addr)) < 0) {
                close(hub_fd);
                hub_fd = -1;
                usleep(500000); // Wait 500ms before retrying
                continue;
            }
            printf("BRAIN: Connected to Sewer Hub.\n");
        }

        // 2. The Non-Blocking Poll
        struct pollfd pfd;
        pfd.fd = hub_fd;
        pfd.events = POLLIN;

        int ret = poll(&pfd, 1, 100); // 100ms timeout
        if (ret > 0 && (pfd.revents & POLLIN)) {
            char buf[1024] = {0};
            ssize_t n = read(hub_fd, buf, sizeof(buf)-1);
            if (n <= 0) {
                printf("BRAIN: Hub disconnected. Reconnecting...\n");
                close(hub_fd);
                hub_fd = -1;
                continue;
            }

            // 3. Routing Logic
            if (strstr(buf, "SCAN")) {
                printf("BRAIN: Routing SCAN request to KRANG.\n");
                dispatch_to_worker(KRANG_PATH, "SCAN");
            } 
            // Future "Hello" or Capability Logic goes here
        }
    }
    return 0;
}
