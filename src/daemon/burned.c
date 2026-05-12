#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>

#define SOCK_PATH "/data/data/com.termux/files/home/tmp/turtlecom.sock"
#define BUF_SIZE 2048

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

static char* run_cmd(const char *cmd) {
    FILE *f = popen(cmd, "r");
    if (!f) return NULL;

    char *buf = malloc(256);
    if (!buf) { pclose(f); return NULL; }

    if (!fgets(buf, 256, f)) {
        free(buf);
        pclose(f);
        return NULL;
    }

    buf[strcspn(buf, "\n")] = 0;
    pclose(f);
    return buf;
}

int main(void) {
    int fd = connect_to_turtlecom();
    char out[BUF_SIZE];

    printf("burned (Burne Thompson MIUI Policy Daemon): ONLINE\n");
    write(fd, "HELLO WORKER BURNE\n", 20);

    for (;;) {
        char *opt = run_cmd("getprop persist.sys.miui_optimization");
        char *bgs = run_cmd("getprop persist.sys.background_data");
        char *pwr = run_cmd("getprop persist.sys.powerkeeper");
        char *auto_start = run_cmd("getprop persist.sys.autostart");
        char *restrict_flag = run_cmd("getprop persist.sys.miui_restricted_mode");
        char *clean = run_cmd("getprop persist.sys.cleaner_level");

        snprintf(out, sizeof(out),
            "STATUS BURNE "
            "OPTIMIZE=%s "
            "BG_DATA=%s "
            "POWERKEEPER=%s "
            "AUTOSTART=%s "
            "RESTRICT=%s "
            "CLEANER=%s\n",

            opt ? opt : "unknown",
            bgs ? bgs : "unknown",
            pwr ? pwr : "unknown",
            auto_start ? auto_start : "unknown",
            restrict_flag ? restrict_flag : "unknown",
            clean ? clean : "unknown"
        );

        write(fd, out, strlen(out));

        free(opt);
        free(bgs);
        free(pwr);
        free(auto_start);
        free(restrict_flag);
        free(clean);

        sleep(60);
    }

    return 0;
}
