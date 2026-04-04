#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define ABS_NAME "sysport_v2"
#define SENT_SOCK "/data/data/com.termux/files/home/MiuiserPeruser/pipes/rocksteady.sock"

void get_sys_data(char *buffer, size_t size) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    addr.sun_path[0] = '\0';
    strncpy(addr.sun_path + 1, ABS_NAME, sizeof(addr.sun_path) - 2);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) == 0) {
        write(sock, "READ /proc/loadavg", 18);
        ssize_t n = read(sock, buffer, size - 1);
        if (n > 0) {
            buffer[n] = '\0';
            char *fin = strstr(buffer, "FIN");
            if (fin) *fin = '\0';
        }
        close(sock);
    } else {
        strncpy(buffer, "OFFLINE", size - 1);
    }
}

int main() {
    int srv = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SENT_SOCK);
    unlink(SENT_SOCK);
    if (bind(srv, (struct sockaddr*)&addr, sizeof(addr)) < 0) return 1;
    listen(srv, 5);

    while (1) {
        int client = accept(srv, NULL, NULL);
        char freq_buf[64] = "0MHz", load_buf[256] = {0};
        char final_tx[512] = {0};

        FILE *f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", "r");
        if (f) { 
            long freq; fscanf(f, "%ld", &freq); fclose(f);
            snprintf(freq_buf, sizeof(freq_buf), "%ldMHz", freq / 1000);
        }

        get_sys_data(load_buf, sizeof(load_buf));
        snprintf(final_tx, sizeof(final_tx), "CPU:%s | LOAD:%s", freq_buf, load_buf);
        send(client, final_tx, strlen(final_tx), 0);
        close(client);
    }
}
