#include "daemon_core.h"
#include <stdio.h>
#include <unistd.h>

int main(void) {
    if (!daemon_core_init("leatherheadd")) return 1;

    for (;;) {
        daemon_log_info("Leatherhead monitoring environment");
        sleep(5);
    }

    daemon_core_shutdown();
    return 0;
}
