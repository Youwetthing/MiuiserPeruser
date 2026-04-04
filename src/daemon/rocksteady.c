#include "fugitoid_log.h"
#include "fugitoid_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fugitoid_log.h"
#include "krang.h"

static void rocksteady_heartbeat(void) {
    
char resp[256];
krang_send_command("echo rocksteady_heartbeat", resp, sizeof(resp));
    fugitoid_log("INFO", "[ROCKSTEADY] Heartbeat sent");
}

int main(void) {
fugitoid_init();

    fugitoid_log("INFO", "[ROCKSTEADY] Starting...");

    int hb_counter = 0;

    for (;;) {
        // TODO: your CPU scanning logic here

        hb_counter++;
        if (hb_counter >= 360) {
            rocksteady_heartbeat();
            hb_counter = 0;
        }

        sleep(30);
    }

    return 0;
}
