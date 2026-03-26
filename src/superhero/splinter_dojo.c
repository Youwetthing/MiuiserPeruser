/*
 * MiuiserPeruser – Splinter Dojo
 * Orchestrator for full scan cycles using Leo + the turtles.
 */

#include "leo_detection.h"
#include "sensei_types.h"
#include "april_platform.h"
#include "april_event.h"
#include "splinter_dojo.h"
#include "backend_strategy.h"

extern void april_log(const char* level, const char* format, ...);

static BACKEND_SELECTION g_backend = { BACKEND_NONE, 0 };

/* Placeholder: in future this will consult Sensei Core */
static SENSEI_STATUS splinter_load_knowledge(void) {
    return SENSEI_STATUS_OK;
}

SENSEI_STATUS splinter_init(void) {
    SENSEI_STATUS st;

    april_log("INFO", "Splinter Dojo: initializing…");

    st = splinter_load_knowledge();
    if (st != SENSEI_STATUS_OK) {
        april_log("CRIT", "Splinter Dojo: failed to load Sensei knowledge (%d)", st);
        return st;
    }

    st = leo_init(NULL);
    if (st != SENSEI_STATUS_OK) {
        april_log("CRIT", "Splinter Dojo: leo_init failed (%d)", st);
        return st;
    }

    april_log("INFO", "Splinter Dojo: initialized.");
    return SENSEI_STATUS_OK;
}

SENSEI_STATUS splinter_run_scan_cycle(uint32_t interval_ms) {
    (void)interval_ms;

    SENSEI_DETECTION_LIST detections = {0};

    april_log("INFO", "Splinter Dojo: turtles, roll out – starting full scan.");

    SENSEI_STATUS st = leo_full_scan(&detections);
    if (st != SENSEI_STATUS_OK) {
        april_log("CRIT", "Splinter Dojo: leo_full_scan failed (%d)", st);
        return st;
    }

    /* Free detection list */
    leo_detection_list_free(&detections);

    /* Backend selection happens AFTER the scan */
    g_backend = backend_select_best();

    april_log("INFO", "Splinter Dojo: full scan cycle complete. Backend=%s",
              backend_name(g_backend.type));

    return SENSEI_STATUS_OK;
}

void splinter_shutdown(void) {
    april_log("INFO", "Splinter Dojo: shutting down…");
    leo_shutdown();
    april_log("INFO", "Splinter Dojo: shutdown complete.");
}
