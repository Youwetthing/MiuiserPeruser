#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

void get_thermal_intel(char *buffer, size_t size) {
    FILE *fp;
    float cpu = 0;
    fp = popen("rish -c 'dumpsys thermalservice | grep \"mName=CPU\" -A 1 | grep \"mValue=\"'", "r");
    if (fp) {
        char buf[128];
        if (fgets(buf, sizeof(buf), fp)) sscanf(strstr(buf, "=") + 1, "%f", &cpu);
        pclose(fp);
    }
    snprintf(buffer, size, "CPU_TEMP:%.1fC | STATUS:%s\n", cpu, (cpu > 45) ? "HOT" : "COOL");
}

int main() {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr = {.sun_family = AF_UNIX};
    strcpy(addr.sun_path, "/data/data/com.termux/files/home/MiuiserPeruser/pipes/leatherhead.sock");
    unlink(addr.sun_path);
    bind(fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(fd, 5);
    while(1) {
        int client = accept(fd, NULL, NULL);
        char tx[256] = {0};
        get_thermal_intel(tx, sizeof(tx));
        send(client, tx, strlen(tx), 0);
        close(client);
    }
    return 0;
}
