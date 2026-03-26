/*
 * MiuiserPeruser – Superhero Mode
 * High-level shell that commands Splinter Dojo.
 */

#include <stdio.h>
#include <unistd.h>
#include "splinter_dojo.h"
#include "superhero_mode.h"
#include "sensei_types.h"

extern void april_log(const char* level, const char* format, ...);

SENSEI_STATUS superhero_run_once(void) {
    SENSEI_STATUS st;

    april_log("INFO", "Superhero Mode: one-and-done scan selected.");

    st = splinter_init();
    if (st != SENSEI_STATUS_OK) {
        april_log("CRIT", "Superhero Mode: splinter_init failed (%d)", st);
        return st;
    }

    st = splinter_run_scan_cycle(0);
    if (st != SENSEI_STATUS_OK) {
        april_log("CRIT", "Superhero Mode: scan cycle failed (%d)", st);
        splinter_shutdown();
        return st;
    }

    splinter_shutdown();
    return SENSEI_STATUS_OK;
}

SENSEI_STATUS superhero_run_loop(uint32_t interval_seconds) {
    SENSEI_STATUS st;

    april_log("INFO", "Superhero Mode: continuous scan loop selected (%u sec interval).",
              interval_seconds);

    st = splinter_init();
    if (st != SENSEI_STATUS_OK) {
        april_log("CRIT", "Superhero Mode: splinter_init failed (%d)", st);
        return st;
    }

    for (;;) {
        st = splinter_run_scan_cycle(interval_seconds * 1000);
        if (st != SENSEI_STATUS_OK) {
            april_log("CRIT", "Superhero Mode: scan cycle failed (%d)", st);
            break;
        }
        sleep(interval_seconds);
    }

    splinter_shutdown();
    return st;
}

SENSEI_STATUS superhero_run_n_times(uint32_t count, uint32_t interval_seconds) {
    SENSEI_STATUS st;

    april_log("INFO",
              "Superhero Mode: bounded scan selected (%u cycles, %u sec interval).",
              count, interval_seconds);

    st = splinter_init();
    if (st != SENSEI_STATUS_OK) {
        april_log("CRIT", "Superhero Mode: splinter_init failed (%d)", st);
        return st;
    }

    for (uint32_t i = 0; i < count; i++) {
        april_log("INFO", "Superhero Mode: cycle %u/%u", i + 1, count);

        st = splinter_run_scan_cycle(interval_seconds * 1000);
        if (st != SENSEI_STATUS_OK) {
            april_log("CRIT", "Superhero Mode: scan cycle failed (%d)", st);
            break;
        }

        if (i + 1 < count)
            sleep(interval_seconds);
    }

    splinter_shutdown();
    return st;
}
