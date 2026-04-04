/*
 * MiuiserPeruser – Superhero Mode
 * High-level shell that commands Splinter Dojo.
 */

#include <stdio.h>
#include <unistd.h>

#include "superhero_mode.h"
#include "splinter_dojo.h"
#include "sensei_types.h"
#include "backend_strategy.h"
#include "april_table.h"

extern void april_log(const char* level, const char* format, ...);

static void log_backend_in_use(void) {
    BACKEND_TYPE b = splinter_get_backend();
    april_log("INFO", "Superhero Mode: backend in use = %s", backend_name(b));
}

static void run_actions(void) {
    splinter_metrics_t m;
    SENSEI_STATUS st = splinter_collect_metrics(&m);
    if (st != SENSEI_STATUS_OK) {
        april_log("WARN", "Superhero Mode: failed to collect metrics (status=%d)", st);
        return;
    }

    april_log("INFO", "Action: thermal=%d", m.thermal);
    april_log("INFO", "Action: battery=%d", m.battery);
    april_log("INFO", "Action: cpu_freq=%d", m.cpu_freq);

    april_table_t *tbl = april_table_create(APRIL_TABLE_NORMAL);
    if (!tbl)
        return;

    char buf_thermal[32];
    char buf_battery[32];
    char buf_cpu[32];

    snprintf(buf_thermal, sizeof(buf_thermal), "%d", m.thermal);
    snprintf(buf_battery, sizeof(buf_battery), "%d", m.battery);
    snprintf(buf_cpu, sizeof(buf_cpu), "%d", m.cpu_freq);

    april_table_add(tbl, "Thermal (mC)",    buf_thermal);
    april_table_add(tbl, "Battery (%)",     buf_battery);
    april_table_add(tbl, "CPU freq (kHz)",  buf_cpu);

    april_table_print(tbl);
    april_table_free(tbl);
}

SENSEI_STATUS superhero_run_once(void) {
    SENSEI_STATUS st;

    april_log("INFO", "Superhero Mode: one-and-done scan selected.");

    st = splinter_init();
    if (st != SENSEI_STATUS_OK)
        return st;

    st = splinter_run_scan_cycle(0);
    if (st != SENSEI_STATUS_OK) {
        splinter_shutdown();
        return st;
    }

    log_backend_in_use();
    run_actions();

    splinter_shutdown();
    return SENSEI_STATUS_OK;
}

SENSEI_STATUS superhero_run_loop(uint32_t interval_seconds) {
    SENSEI_STATUS st;

    april_log("INFO", "Superhero Mode: continuous scan loop (%u sec).",
              interval_seconds);

    st = splinter_init();
    if (st != SENSEI_STATUS_OK)
        return st;

    for (;;) {
        st = splinter_run_scan_cycle(interval_seconds * 1000);
        if (st != SENSEI_STATUS_OK)
            break;

        log_backend_in_use();
        run_actions();
        sleep(interval_seconds);
    }

    splinter_shutdown();
    return st;
}

SENSEI_STATUS superhero_run_n_times(uint32_t count, uint32_t interval_seconds) {
    SENSEI_STATUS st;

    april_log("INFO", "Superhero Mode: bounded scan (%u cycles, %u sec).",
              count, interval_seconds);

    st = splinter_init();
    if (st != SENSEI_STATUS_OK)
        return st;

    for (uint32_t i = 0; i < count; i++) {
        april_log("INFO", "Superhero Mode: cycle %u/%u", i + 1, count);

        st = splinter_run_scan_cycle(interval_seconds * 1000);
        if (st != SENSEI_STATUS_OK)
            break;

        log_backend_in_use();
        run_actions();

        if (i + 1 < count)
            sleep(interval_seconds);
    }

    splinter_shutdown();
    return st;
}
