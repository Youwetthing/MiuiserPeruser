#include "superhero/superhero_mode.h"
#include "leo_detection.h"
#include "fugitoid_log.h"
#include <unistd.h>
#include <stdio.h>

static SENSEI_STATUS execute_scan_cycle() {
    SENSEI_DETECTION_LIST results = {0};
    fugitoid_log("INFO", "--- Superhero Scan Initiated ---");
    
    SENSEI_STATUS status = leo_full_scan(&results);
    
    if (status == SENSEI_STATUS_OK) {
        fugitoid_log("INFO", "Scan completed. Findings: %d", results.count);
    } else {
        fugitoid_log("ERROR", "Scan cycle failed.");
    }

    leo_detection_list_free(&results);
    return status;
}

SENSEI_STATUS superhero_run_once(void) {
    return execute_scan_cycle();
}

SENSEI_STATUS superhero_run_n_times(uint32_t count, uint32_t interval_seconds) {
    for (uint32_t i = 0; i < count; i++) {
        execute_scan_cycle();
        if (i < count - 1) sleep(interval_seconds);
    }
    return SENSEI_STATUS_OK;
}

SENSEI_STATUS superhero_run_loop(uint32_t interval_seconds) {
    while (1) {
        execute_scan_cycle();
        sleep(interval_seconds);
    }
    return SENSEI_STATUS_OK;
}
