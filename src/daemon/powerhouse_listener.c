#include <stdio.h>
#include "action_matrix.h"

// Tell the compiler this function exists elsewhere
extern int dispatch_to_sewer(action_id_t action_id);

void run_worker_poll() {
    static int cycle_count = 0;
    cycle_count++;

    // Every 5 cycles, simulate a "High Load" event
    if (cycle_count % 5 == 0) {
        printf("[BRAIN] ALERT: High CPU Load Detected! Dispatching Optimize Command...\n");
        dispatch_to_sewer(ACTION_OPTIMIZE_CORES);
    } else {
        printf("[BRAIN] System health within parameters.\n");
    }
}
