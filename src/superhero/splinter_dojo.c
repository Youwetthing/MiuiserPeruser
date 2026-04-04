#include "splinter_dojo.h"
#include "april_event.h"
#include "backend_common.h"

extern void april_log(const char* level, const char* format, ...);

static BACKEND_TYPE g_backend = BACKEND_NONE;

SENSEI_STATUS splinter_init(void) {
    april_log("INFO", "Splinter Dojo: initializing backend…");

    g_backend = backend_strategy_select();

    /* Emit backend selection event */
    april_emit_event(APRIL_EVENT_BACKEND_SELECTED, (int)g_backend);

    return SENSEI_STATUS_OK;
}

SENSEI_STATUS splinter_run_scan_cycle(uint32_t interval_ms) {
    april_log("INFO", "Splinter Dojo: scan cycle (%u ms)", interval_ms);

    april_emit_event(APRIL_EVENT_SCAN_START, 0);

    (void)interval_ms;

    april_emit_event(APRIL_EVENT_SCAN_END, 0);

    return SENSEI_STATUS_OK;
}

SENSEI_STATUS splinter_collect_metrics(splinter_metrics_t *out) {
    if (!out)
        return SENSEI_STATUS_ERROR;

    const backend_vtable_t *vt = backend_get_active_vtable();
    if (!vt)
        return SENSEI_STATUS_ERROR;

    int t = 0, b = 0, c = 0;

    vt->read_thermal(&t);
    vt->read_battery(&b);
    vt->read_cpu_freq(&c);

    out->thermal  = t;
    out->battery  = b;
    out->cpu_freq = c;

    /* Emit metric events */
    april_emit_event(APRIL_EVENT_METRIC_THERMAL,  t);
    april_emit_event(APRIL_EVENT_METRIC_BATTERY,  b);
    april_emit_event(APRIL_EVENT_METRIC_CPUFREQ,  c);

    return SENSEI_STATUS_OK;
}

void splinter_shutdown(void) {
    april_log("INFO", "Splinter Dojo: shutdown.");
}

BACKEND_TYPE splinter_get_backend(void) {
    return g_backend;
}
