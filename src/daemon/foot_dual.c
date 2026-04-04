#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <poll.h>

void get_mental_health(char *buffer, size_t size) {
    long mem_total = 0, mem_avail = 0, swap_total = 0, swap_free = 0;
    int proc_count = 0;
    FILE *fp;
    char line[256];

    if ((fp = fopen("/proc/meminfo", "r"))) {
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "MemTotal:", 9) == 0) sscanf(line, "MemTotal: %ld", &mem_total);
            if (strncmp(line, "MemAvailable:", 13) == 0) sscanf(line, "MemAvailable: %ld", &mem_avail);
            if (strncmp(line, "SwapTotal:", 10) == 0) sscanf(line, "SwapTotal: %ld", &swap_total);
            if (strncmp(line, "SwapFree:", 9) == 0) sscanf(line, "SwapFree: %ld", &swap_free);
        }
        fclose(fp);
    }

    fp = popen("ps -A | wc -l", "r");
    if (fp) {
        if (fgets(line, sizeof(line), fp)) proc_count = atoi(line);
        pclose(fp);
    }

    snprintf(buffer, size, 
        "SCOUT: MEM:%ld/%ld MB | SWAP:%ld MB | PROCS:%d\n", 
        mem_avail/1024, mem_total/1024, (swap_total - swap_free)/1024, proc_count);
}

int main() {
    int sewer_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un sewer_addr = {.sun_family = AF_UNIX};
    strcpy(sewer_addr.sun_path, "/data/data/com.termux/files/home/MiuiserPeruser/pipes/foot1.sock");
    unlink(sewer_addr.sun_path);
    bind(sewer_fd, (struct sockaddr*)&sewer_addr, sizeof(sewer_addr));
    listen(sewer_fd, 5);

    int city_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in city_addr = {0};
    city_addr.sin_family = AF_INET;
    city_addr.sin_port = htons(8082);
    city_addr.sin_addr.s_addr = INADDR_ANY;
    bind(city_fd, (struct sockaddr*)&city_addr, sizeof(city_addr));
    listen(city_fd, 5);

    printf("FOOT_1: Scout V12 Active (8082/Sewer). Reporting mental health...\n");

    struct pollfd fds[2] = { {sewer_fd, POLLIN}, {city_fd, POLLIN} };

    while(1) {
        poll(fds, 2, -1);
        for(int i=0; i<2; i++) {
            if(fds[i].revents & POLLIN) {
                int client = accept(fds[i].fd, NULL, NULL);
                char rx[64] = {0}, tx[256] = {0};
                read(client, rx, 63);
                get_mental_health(tx, sizeof(tx));
                send(client, tx, strlen(tx), 0);
                close(client);
            }
        }
    }
    return 0;
}
