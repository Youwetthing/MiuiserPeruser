#include "fugitoid_log.h"
#include <unistd.h>

int main(void) {
    fugitoid_init();
    fugitoid_log("INFO", "[PORT] foot_portwatchd online (Port 42424)");

    for (;;) {
        fugitoid_log("DEBUG", "[PORT] Sentry heartbeat: monitoring socket...");
        sleep(15);
    }
    return 0;
}
