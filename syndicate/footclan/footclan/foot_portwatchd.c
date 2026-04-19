#include "fugitoid_log.h"
#include "krang.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * FOOT PORT WATCHER
 * -----------------
 * This is a tiny fallback worker that Splinter can spawn.
 * For now it just logs that it is alive.
 */

int main(void) {
    fugitoid_init();
    fugitoid_log("INFO", "[FOOT] foot_portwatchd online");

    while (1) {
        fugitoid_log("DEBUG", "[FOOT] port watcher heartbeat");
        sleep(5);
    }

    return 0;
}
