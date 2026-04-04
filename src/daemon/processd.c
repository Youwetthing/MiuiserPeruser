#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

void get_process_intel(char *buffer, size_t size) {
    FILE *fp;
    char buf[128];
    float sys_load = 0.0, io_wait = 0.0;

    /* Scrape the CPU summary line for the REAL truth */
    fp = popen("adb -s 192.168.0.45:5555 shell top -b -n 1 | grep 'cpu '", "r");
    if (fp) {
        if (fgets(buf, sizeof(buf), fp)) {
            // Looking for "34%sys" and "0%iow"
            char *s = strstr(buf, "sys");
            if (s) sscanf(s - 4, "%f", &sys_load);
            char *i = strstr(buf, "iow");
            if (i) sscanf(i - 4, "%f", &io_wait);
        }
        pclose(fp);
    }

    snprintf(buffer, size, "SYS_LOAD:%.1f%% | IOWait:%.1f%% | STATUS:%s\n", 
             sys_load, io_wait, (sys_load > 20.0) ? "KERNEL_THRASH" : "HEALTHY");
}

int main() {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr = {.sun_family = AF_UNIX};
    strcpy(addr.sun_path, "/data/data/com.termux/files/home/MiuiserPeruser/pipes/ratking.sock");
    unlink(addr.sun_path);
    bind(fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(fd, 5);
    while(1) {
        int client = accept(fd, NULL, NULL);
        char tx[256] = {0};
        get_process_intel(tx, sizeof(tx));
        send(client, tx, strlen(tx), 0);
        close(client);
    }
    return 0;
}
