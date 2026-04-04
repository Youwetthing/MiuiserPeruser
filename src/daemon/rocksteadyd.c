#include "syndicate_capabilities.h"
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
    if (!daemon_core_init("rocksteadyd")) return 1;

    int fd = connect_bus();
    if (fd < 0) return 1;

    write(fd, "HELLO WORKER ROCKSTEADY\n", 25);
    daemon_log_info("rocksteadyd ONLINE");

    char buf[256];
    int hb_counter = 0;

    for (;;) {
        /* Heartbeat every ~30s */
        hb_counter++;
        if (hb_counter >= 300) {
            write(fd, "HEARTBEAT ROCKSTEADY\n", 22);
            hb_counter = 0;
        }

        int n = read(fd, buf, sizeof(buf)-1);
        if (n <= 0) { usleep(100000); continue; }
        buf[n] = 0;

        /* Capabilities */
        if (strncmp(buf, "CAPABILITY?", 11) == 0) {
            write(fd, "CAPABILITY HEARTBEAT SEND\n", 27);
            write(fd, "CAPABILITY CPU TEMP\n", 21);
            continue;
        }

        /* HEARTBEAT SEND */
        if (strncmp(buf, "HEARTBEAT SEND", 14) == 0) {
            write(fd, "HEARTBEAT ROCKSTEADY\n", 22);
            continue;
        }

        /* CPU TEMP */
        if (strncmp(buf, "CPU TEMP", 8) == 0) {
            char *t = run("cat /sys/class/thermal/thermal_zone0/temp 2>/dev/null");
            dprintf(fd, "CPU TEMP %s\n", t ? t : "unknown");
            continue;
        }
    }

    return 0;
}
