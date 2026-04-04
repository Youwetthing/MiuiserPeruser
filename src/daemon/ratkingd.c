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
    if (!daemon_core_init("ratkingd")) return 1;

    int fd = connect_bus();
    if (fd < 0) return 1;

    write(fd, "HELLO WORKER RATKING\n", 22);
    daemon_log_info("ratkingd ONLINE");

    char buf[256];

    for (;;) {
        int n = read(fd, buf, sizeof(buf)-1);
        if (n <= 0) { usleep(100000); continue; }
        buf[n] = 0;

        /* Capabilities */
        if (strncmp(buf, "CAPABILITY?", 11) == 0) {
            write(fd, "CAPABILITY NOISE SCAN\n", 23);
            write(fd, "CAPABILITY ENTROPY CHECK\n", 26);
            continue;
        }

        /* NOISE SCAN */
        if (strncmp(buf, "NOISE SCAN", 10) == 0) {
            char *ps = run("ps -A | wc -l");
            char *iostat = run("iostat 2>/dev/null | head -n3 | tail -n1");
            dprintf(fd, "NOISE SCAN PROCS=%s IO=%s\n",
                ps ? ps : "unknown",
                iostat ? iostat : "unknown");
            continue;
        }

        /* ENTROPY CHECK */
        if (strncmp(buf, "ENTROPY CHECK", 13) == 0) {
            char *entropy = run("cat /proc/sys/kernel/random/entropy_avail");
            dprintf(fd, "ENTROPY CHECK %s\n", entropy ? entropy : "unknown");
            continue;
        }
    }

    return 0;
}
