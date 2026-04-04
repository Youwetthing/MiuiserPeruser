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
    static char buf[256];
    if (!fgets(buf, sizeof(buf), f)) {
        pclose(f);
        return NULL;
    }
    buf[strcspn(buf, "\n")] = 0;
    pclose(f);
    return buf;
}

int main(void) {
    if (!daemon_core_init("leatherheadd")) return 1;

    int fd = connect_bus();
    if (fd < 0) return 1;

    write(fd, "HELLO WORKER LEATHERHEAD\n", 26);
    daemon_log_info("leatherheadd ONLINE");

    char buf[256];

    for (;;) {
        int n = read(fd, buf, sizeof(buf)-1);
        if (n <= 0) { usleep(100000); continue; }
        buf[n] = 0;

        /* Capabilities */
        if (strncmp(buf, "CAPABILITY?", 11) == 0) {
            write(fd, "CAPABILITY NET STATUS\n", 23);
            write(fd, "CAPABILITY NET SIGNAL\n", 23);
            write(fd, "CAPABILITY NET LATENCY\n", 24);
            continue;
        }

        /* NET STATUS */
        if (strncmp(buf, "NET STATUS", 10) == 0) {
            char *wifi = run("getprop sys.wifi_on");
            char *data = run("getprop gsm.defaultpdpcontext.active");
            dprintf(fd, "NET STATUS WIFI=%s DATA=%s\n",
                wifi ? wifi : "unknown",
                data ? data : "unknown");
            continue;
        }

        /* NET SIGNAL */
        if (strncmp(buf, "NET SIGNAL", 10) == 0) {
            char *sig = run("dumpsys telephony.registry | grep mSignalStrength | head -n1");
            dprintf(fd, "NET SIGNAL %s\n", sig ? sig : "unknown");
            continue;
        }

        /* NET LATENCY */
        if (strncmp(buf, "NET LATENCY", 11) == 0) {
            char *ping = run("ping -c 1 8.8.8.8 | grep time= | awk -F'time=' '{print $2}'");
            dprintf(fd, "NET LATENCY %s\n", ping ? ping : "unknown");
            continue;
        }
    }

    return 0;
}
