#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>

#define KRANG_PATH "/data/data/com.termux/files/home/MiuiserPeruser/pipes/krang.sock"
#define HUB_PATH "/data/data/com.termux/files/home/MiuiserPeruser/pipes/turtlecom.sock"

int discover_serial(char *out_serial, size_t max_len) {
    FILE *fp = popen("adb devices | grep -v 'emulator' | grep 'device$' | head -n 1 | awk '{print $1}'", "r");
    if (!fp) return 0;
    if (fgets(out_serial, max_len, fp) == NULL) {
        pclose(fp);
        return 0;
    }
    pclose(fp);
    out_serial[strcspn(out_serial, "\n")] = 0;
    return (strlen(out_serial) > 0);
}

void harvest(int hub_fd, const char* serial) {
    char buf[512], cmd[512];
    snprintf(cmd, sizeof(cmd), "adb -s %s shell dumpsys thermalservice | grep 'Thermal Status'", serial);
    FILE *fp = popen(cmd, "r");
    if (fp) {
        while (fgets(buf, sizeof(buf), fp)) dprintf(hub_fd, "SOB_THERM: %s", buf);
        pclose(fp);
    }
}

int main() {
    char serial[128] = {0};
    unlink(KRANG_PATH);

    printf("KRANG: Muscle waiting for MIUI device...\n");
    while (!discover_serial(serial, sizeof(serial))) sleep(1);
    printf("KRANG: Locked onto [%s]\n", serial);

    int serv_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, KRANG_PATH, sizeof(addr.sun_path)-1);
    bind(serv_fd, (struct sockaddr*)&addr, sizeof(addr));
    chmod(KRANG_PATH, 0666);
    listen(serv_fd, 5);

    while (1) {
        int conn_fd = accept(serv_fd, NULL, NULL);
        if (conn_fd >= 0) {
            char cmd_in[128] = {0};
            read(conn_fd, cmd_in, sizeof(cmd_in));
            if (strstr(cmd_in, "SCAN")) {
                int hub_fd = socket(AF_UNIX, SOCK_STREAM, 0);
                struct sockaddr_un h_addr;
                memset(&h_addr, 0, sizeof(h_addr));
                h_addr.sun_family = AF_UNIX;
                strncpy(h_addr.sun_path, HUB_PATH, sizeof(h_addr.sun_path)-1);
                if (connect(hub_fd, (struct sockaddr*)&h_addr, sizeof(h_addr)) == 0) {
                    harvest(hub_fd, serial);
                    close(hub_fd);
                }
            }
            close(conn_fd);
        }
    }
    return 0;
}

int krang_send_command(const char *cmd) {
    if (!cmd) return -1;
    printf("[KRANG] Relaying command: %s\n", cmd);
    return 0;
}
