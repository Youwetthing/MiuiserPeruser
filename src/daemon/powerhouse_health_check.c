#include <stdio.h>
#include <unistd.h>
#include "syndicate_paths.h"
#include "syndicate_health.h"

// Initialize the health monitor
health_monitor_t shadow_health = {STATUS_OFFLINE, 0};

void check_shadow_vitality() {
    // 1. Drop the Ping
    FILE *q = fopen(Q_READY, "w");
    if (q) {
        fprintf(q, "ping");
        fflush(q);
        fclose(q);
    }

    // 2. Give the Muscle a half-second to react
    usleep(500000); 

    // 3. Check for the Pong
    if (access(A_READY, F_OK) == 0) {
        shadow_health.state = STATUS_HEALTHY;
        shadow_health.missed_pings = 0;
        remove(A_READY);
    } else {
        shadow_health.missed_pings++;
        if (shadow_health.missed_pings >= 3) {
            shadow_health.state = STATUS_OFFLINE;
        }
    }
}
