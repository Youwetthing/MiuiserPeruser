#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <dirent.h>

#define SOCK_PATH "/data/data/com.termux/files/home/tmp/turtlecom.sock"
#define BUF_SIZE 512

static int connect_to_turtlecom(void) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); exit(1); }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCK_PATH);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(fd);
        exit(1);
    }

    fcntl(fd, F_SETFL, O_NONBLOCK);
    return fd;
}

static int read_temp(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    int t = -1;
    fscanf(f, "%d", &t);
    fclose(f);
    return t / 1000; // convert millideg to °C
}

int main(void) {
    int fd = connect_to_turtlecom();
    char buf[BUF_SIZE];

    write(fd, "HELLO WORKER LEATHERHEAD\n", 26);
    printf("leatherheadd (Leatherhead): ONLINE\n");

    for (;;) {
        int cpu = -1, batt = -1, gpu = -1, skin = -1, modem = -1;

        DIR *d = opendir("/sys/class/thermal");
        if (d) {
            struct dirent *e;
            while ((e = readdir(d))) {
                if (strncmp(e->d_name, "thermal_zone", 12) != 0)
                    continue;

                char typepath[256], temppath[256], type[64];
                snprintf(typepath, sizeof(typepath),
                         "/sys/class/thermal/%s/type", e->d_name);
                snprintf(temppath, sizeof(temppath),
                         "/sys/class/thermal/%s/temp", e->d_name);

                FILE *tf = fopen(typepath, "r");
                if (!tf) continue;

                if (fgets(type, sizeof(type), tf)) {
                    type[strcspn(type, "\n")] = 0;
                }
                fclose(tf);

                int t = read_temp(temppath);
                if (t < 0) continue;

                if (strcasestr(type, "cpu"))
                    cpu = t;
                else if (strcasestr(type, "battery"))
                    batt = t;
                else if (strcasestr(type, "gpu"))
                    gpu = t;
                else if (strcasestr(type, "skin"))
                    skin = t;
                else if (strcasestr(type, "modem"))
                    modem = t;
            }
            closedir(d);
        }

        int hot = (cpu > 70 || batt > 45) ? 1 : 0;

        snprintf(buf, sizeof(buf),
                 "STATUS LEATHERHEAD CPU=%d BATTERY=%d GPU=%d SKIN=%d MODEM=%d HOT=%d\n",
                 cpu, batt, gpu, skin, modem, hot);

        write(fd, buf, strlen(buf));
        usleep(1000000);
    }

    return 0;
}
