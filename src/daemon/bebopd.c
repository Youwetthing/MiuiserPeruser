#include "daemon_core.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>

#define BUS_PATH "/data/data/com.termux/files/home/tmp/turtlecom.sock"

int main(void) {
    if (!daemon_core_init("bebopd")) return 1;

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, BUS_PATH);

    // Try to connect to the Sewer
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        daemon_log_error("Bebop cannot find the Sewer Hub!");
        return 1;
    }

    // Identify to the Hub
    dprintf(fd, "HELLO WORKER BEBOP\n");
    daemon_log_info("Bebop (The Muscle) ONLINE - Waiting for the Brain...");

    char buf[1024];
    for (;;) {
        ssize_t n = read(fd, buf, sizeof(buf)-1);
        if (n <= 0) {
            daemon_log_error("Sewer connection lost!");
            break;
        }
        buf[n] = 0;

        /* LOGIC: Only act if Splinter routed it to us OR it's a direct 'CPU LOAD' */
        if (strstr(buf, "TO BEBOPD") || (strstr(buf, "CPU LOAD") && !strstr(buf, "TO "))) {
            daemon_log_info("Command Received: %s", buf);

            // Execute the system probe
            FILE *f = popen("top -bn1 | grep 'CPU' | head -n1", "r");
            if (f) {
                char result[256];
                if (fgets(result, sizeof(result), f)) {
                    // Send it back to the bus for everyone (including NC) to see
                    dprintf(fd, "BEBOP_REPORT: %s", result);
                }
                pclose(f);
            }
        }
    }

    return 0;
}
