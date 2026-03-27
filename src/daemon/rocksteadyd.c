#include "daemon_core.h"
#include <stdio.h>
#include <unistd.h>

int main(void) {
    if (!daemon_core_init("rocksteadyd")) return 1;

    for (;;) {
        daemon_log_info("Rocksteady standing guard");
        sleep(5);
    }

    daemon_core_shutdown();
    return 0;
}
