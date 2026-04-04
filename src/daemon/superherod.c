#include "daemon_core.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>

#define BUS_PATH "/data/data/com.termux/files/home/tmp/turtlecom.sock"

static int connect_bus(void) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, BUS_PATH);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    fcntl(fd, F_SETFL, O_NONBLOCK);
    return fd;
}

static char* run(const char *cmd) {
    FILE *f = popen(cmd, "r");
    if (!f) return NULL;
    static char buf[512];
    if (!fgets(buf, sizeof(buf), f)) {
        pclose(f);
        return NULL;
    }
    buf[strcspn(buf, "\n")] = 0;
    pclose(f);
    return buf;
}

int main(void) {
    if (!daemon_core_init("superherod")) return 1;

    int fd = connect_bus();
    if (fd < 0) return 1;

    write(fd, "HELLO WORKER SUPERHERO\n", 24);
    daemon_log_info("superherod ONLINE");

    char buf[512];

    for (;;) {
        int n = read(fd, buf, sizeof(buf)-1);
        if (n <= 0) { usleep(100000); continue; }
        buf[n] = 0;

        /* Capabilities */
        if (strncmp(buf, "CAPABILITY?", 11) == 0) {
            write(fd, "CAPABILITY SCAN FULL\n", 22);
            write(fd, "CAPABILITY SCAN QUICK\n", 23);
            continue;
        }

        /* SCAN QUICK */
        if (strncmp(buf, "SCAN QUICK", 10) == 0) {
            char *cpu = run("top -bn1 | grep 'CPU' | awk '{print $2}'");
            char *temp = run("cat /sys/class/thermal/thermal_zone0/temp 2>/dev/null");
            dprintf(fd, "SCAN QUICK CPU=%s TEMP=%s\n",
                cpu ? cpu : "unknown",
                temp ? temp : "unknown");
            continue;
        }

        /* SCAN FULL */
        if (strncmp(buf, "SCAN FULL", 9) == 0) {
            char *cpu = run("top -bn1 | grep 'CPU' | awk '{print $2}'");
            char *temp = run("cat /sys/class/thermal/thermal_zone0/temp 2>/dev/null");
            char *df = run("df -h / | tail -n1");
            char *entropy = run("cat /proc/sys/kernel/random/entropy_avail");
            char *wifi = run("getprop sys.wifi_on");

            dprintf(fd,
                "SCAN FULL CPU=%s TEMP=%s DISK=%s ENTROPY=%s WIFI=%s\n",
                cpu ? cpu : "unknown",
                temp ? temp : "unknown",
                df ? df : "unknown",
                entropy ? entropy : "unknown",
                wifi ? wifi : "unknown"
            );
            continue;
        }
    }

    return 0;
}
