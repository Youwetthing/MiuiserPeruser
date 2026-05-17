#include "daemon_core.h"
#include <stdio.h>
#include <unistd.h>

int main(void) {
    if (!daemon_core_init("metalheadd")) return 1;

    for (;;) {
        daemon_log_info("Metalhead scanning sensors");
        sleep(2);
    }

    daemon_core_shutdown();
    return 0;
}
