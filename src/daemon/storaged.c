#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

void get_disk_intel(char *buffer, size_t size) {
    FILE *fp;
    char buf[256];
    float speed = 0;
    fp = popen("sh ~/MiuiserPeruser/src/daemon/rish -c 'dumpsys diskstats | grep \"Write Speed\"'", "r");
    if (fp) {
        if (fgets(buf, sizeof(buf), fp)) {
            char *val = strstr(buf, "=");
            if (val) sscanf(val + 1, "%f", &speed);
        }
        pclose(fp);
    }
    snprintf(buffer, size, "DISK_I/O:%.0fkB/s\n", speed);
}

int main() {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr = {.sun_family = AF_UNIX};
    strcpy(addr.sun_path, "/data/data/com.termux/files/home/MiuiserPeruser/pipes/metal.sock");
    unlink(addr.sun_path);
    bind(fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(fd, 5);
    while(1) {
        int client = accept(fd, NULL, NULL);
        char tx[256] = {0};
        get_disk_intel(tx, sizeof(tx));
        send(client, tx, strlen(tx), 0);
        close(client);
    }
    return 0;
}
