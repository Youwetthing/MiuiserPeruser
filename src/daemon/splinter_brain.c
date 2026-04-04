#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define KRANG_PATH "/data/data/com.termux/files/home/MiuiserPeruser/pipes/krang.sock"

void splinter_brain_handle_line(int hub_fd, const char *line) {
    if (strstr(line, "SCAN")) {
        // Direct private pipe to Krang
        int kfd = socket(AF_UNIX, SOCK_STREAM, 0);
        struct sockaddr_un addr = {0, AF_UNIX};
        strncpy(addr.sun_path, KRANG_PATH, sizeof(addr.sun_path)-1);
        
        if (connect(kfd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            dprintf(kfd, "SCAN\n");
            close(kfd);
        }
    }
}
