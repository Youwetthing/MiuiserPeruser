#include "daemon_core.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>

#define BUS_PATH "/data/data/com.termux/files/home/tmp/turtlecom.sock"
#define BUF_SIZE 2048

static int connect_bus(void) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
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
    if (!daemon_core_init("burned")) return 1;

    int fd = connect_bus();
    if (fd < 0) return 1;

    write(fd, "HELLO WORKER BURNE\n", 20);
    daemon_log_info("burned ONLINE");

    char buf[BUF_SIZE];

    for (;;) {
        int n = read(fd, buf, sizeof(buf)-1);
        if (n <= 0) {
            usleep(100000);
            continue;
        }
        buf[n] = 0;

        /* Capability discovery */
        if (strncmp(buf, "CAPABILITY?", 11) == 0) {
            write(fd, "CAPABILITY POLICY MIUI\n", 24);
            write(fd, "CAPABILITY POLICY AUTOSTART\n", 29);
            write(fd, "CAPABILITY POLICY POWER\n", 24);
            continue;
        }

        /* POLICY MIUI */
        if (strncmp(buf, "POLICY MIUI", 11) == 0) {
            char *opt = run("getprop persist.sys.miui_optimization");
            char *restrict_flag = run("getprop persist.sys.miui_restricted_mode");
            dprintf(fd, "POLICY MIUI OPT=%s RESTRICTED=%s\n",
                opt ? opt : "unknown",
                restrict_flag ? restrict_flag : "unknown");
            continue;
        }

        /* POLICY AUTOSTART */
        if (strncmp(buf, "POLICY AUTOSTART", 16) == 0) {
            char *auto_start = run("getprop persist.sys.autostart");
            dprintf(fd, "POLICY AUTOSTART %s\n",
                auto_start ? auto_start : "unknown");
            continue;
        }

        /* POLICY POWER */
        if (strncmp(buf, "POLICY POWER", 12) == 0) {
            char *pwr = run("getprop persist.sys.powerkeeper");
            char *bgs = run("getprop persist.sys.background_data");
            dprintf(fd, "POLICY POWER POWERKEEPER=%s BGDATA=%s\n",
                pwr ? pwr : "unknown",
                bgs ? bgs : "unknown");
            continue;
        }
    }

    return 0;
}
