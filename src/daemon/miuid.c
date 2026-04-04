#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

void get_miui_news(char *buffer, size_t size) {
    FILE *fp;
    char buf[256];
    char top_service[64] = "IDLE";
    int bg_count = 0;

    // 1. Get the Top Activity
    fp = popen("adb -s 192.168.0.45:5555 shell dumpsys activity top | grep 'ACTIVITY' | tail -n 1", "r");
    if (fp) {
        if (fgets(buf, sizeof(buf), fp)) {
            char *p = strchr(buf, '/');
            if (p) {
                *p = '\0';
                char *start = strrchr(buf, ' ');
                if (start) {
                    strncpy(top_service, start + 1, sizeof(top_service)-1);
                }
            }
        }
        pclose(fp);
    }

    // 2. Count MIUI background sync/daemons
    fp = popen("adb -s 192.168.0.45:5555 shell ps -ef | grep -E 'miui|xiaomi|daemon' | grep -v grep | wc -l", "r");
    if (fp) {
        if (fscanf(fp, "%d", &bg_count) != 1) bg_count = 0;
        pclose(fp);
    }

    snprintf(buffer, size, "FRONT:%s | MI_DAEMONS:%d | STATUS:%s\n", 
             top_service, bg_count, (bg_count > 10) ? "CROWDED" : "LIGHT");
}

int main() {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr = {.sun_family = AF_UNIX};
    strcpy(addr.sun_path, "/data/data/com.termux/files/home/MiuiserPeruser/pipes/burne.sock");
    unlink(addr.sun_path);
    bind(fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(fd, 5);

    while(1) {
        int client = accept(fd, NULL, NULL);
        char tx[256] = {0};
        get_miui_news(tx, sizeof(tx));
        send(client, tx, strlen(tx), 0);
        close(client);
    }
    return 0;
}
