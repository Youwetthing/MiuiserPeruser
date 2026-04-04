#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <poll.h>

typedef struct {
    float temp_cpu;
    int batt_level;
    long cpu_freq;
    int mit_count;
} Telemetry;

void harvest(Telemetry *t) {
    FILE *fp;
    char buf[256];
    t->temp_cpu = 0;

    // 1. Primary: ADB Dumpsys Fallback (The High-Fidelity source)
    // We use the specific device IP found in your environment
    fp = popen("adb -s 192.168.0.45:5555 shell dumpsys thermalservice 2>/dev/null | grep -m1 'mName=CPU' | cut -d= -f2 | cut -d, -f1", "r");
    if (fp) {
        if (fgets(buf, sizeof(buf), fp)) t->temp_cpu = atof(buf);
        pclose(fp);
    }

    // 2. Battery (System Property)
    fp = popen("getprop debug.device.battery_level_state", "r");
    if (fp) {
        if (fgets(buf, sizeof(buf), fp)) t->batt_level = atoi(buf);
        pclose(fp);
    }

    // 3. CPU Freq (Kernel allows this)
    if ((fp = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", "r"))) {
        if(fscanf(fp, "%ld", &t->cpu_freq) != 1) t->cpu_freq = 0;
        fclose(fp);
    }

    // 4. Mitigations
    fp = popen("getprop sys.rescuepartyplus.temp_mitigation_count", "r");
    if (fp) {
        if (fgets(buf, sizeof(buf), fp)) t->mit_count = atoi(buf);
        pclose(fp);
    }
}

int main() {
    int sewer_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un sewer_addr = {.sun_family = AF_UNIX};
    strcpy(sewer_addr.sun_path, "/data/data/com.termux/files/home/MiuiserPeruser/pipes/krang.sock");
    unlink(sewer_addr.sun_path);
    bind(sewer_fd, (struct sockaddr*)&sewer_addr, sizeof(sewer_addr));
    listen(sewer_fd, 5);

    int city_city_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in city_addr = {.sin_family = AF_INET, .sin_port = htons(8081), .sin_addr.s_addr = INADDR_ANY};
    bind(city_city_fd, (struct sockaddr*)&city_addr, sizeof(city_addr));
    listen(city_city_fd, 5);

    struct pollfd fds[2] = { {sewer_fd, POLLIN}, {city_city_fd, POLLIN} };
    while(1) {
        poll(fds, 2, -1);
        for(int i=0; i<2; i++) {
            if(fds[i].revents & POLLIN) {
                int client = accept(fds[i].fd, NULL, NULL);
                char rx[64] = {0}, tx[256] = {0};
                read(client, rx, 63);
                Telemetry t = {0};
                harvest(&t);
                snprintf(tx, sizeof(tx), "HARDWARE: TEMP:%.1fC | BATT:%d%% | FREQ:%ldMHz | MIT:%d\n",
                         t.temp_cpu, t.batt_level, t.cpu_freq/1000, t.mit_count);
                send(client, tx, strlen(tx), 0);
                close(client);
            }
        }
    }
    return 0;
}
