#include "splinter_dojo.h"
#include "april_event.h"
#include "backend_common.h"
#include "leo_detection.h"
#include "rish_pipe.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

extern void april_log(const char* level, const char* format, ...);

static BACKEND_TYPE g_backend = BACKEND_NONE;

SENSEI_STATUS splinter_init(void) {
    april_log("INFO", "Splinter Dojo: initializing backend...");
    g_backend = backend_strategy_select();
    april_emit_event(APRIL_EVENT_BACKEND_SELECTED, (int)g_backend);
    leo_init(NULL);
    return SENSEI_STATUS_OK;
}

SENSEI_STATUS splinter_run_scan_cycle(uint32_t interval_ms) {
    (void)interval_ms;
    april_log("INFO", "Splinter Dojo: starting full scan cycle");
    april_emit_event(APRIL_EVENT_SCAN_START, 0);

    SENSEI_DETECTION_LIST results = {0};
    leo_full_scan(&results);
    leo_detection_list_free(&results);

    april_emit_event(APRIL_EVENT_SCAN_END, 0);
    april_log("INFO", "Splinter Dojo: scan cycle complete");
    return SENSEI_STATUS_OK;
}

SENSEI_STATUS splinter_collect_metrics(splinter_metrics_t *out) {
    if (!out) return SENSEI_STATUS_ERROR;

    // --- Battery: use dumpsys battery ---
    char *battery_raw = rish_pipe_command("dumpsys battery");
    out->battery = -1;
    if (battery_raw) {
        char *level = strstr(battery_raw, "level:");
        if (level) {
            out->battery = atoi(level + 6);
        }
        free(battery_raw);
    }

    // --- Thermal: use dumpsys thermalservice ---
    char *thermal_raw = rish_pipe_command("dumpsys thermalservice | grep -E 'temperature|battery' | head -1");
    out->thermal = -1;
    if (thermal_raw) {
        char *num = thermal_raw;
        while (*num && !isdigit(*num) && *num != '-') num++;
        if (*num) out->thermal = atoi(num);
        free(thermal_raw);
    }

    // --- CPU Freq: use the correct policy6 path you discovered ---
    char *cpu_raw = rish_pipe_command("cat /sys/devices/system/cpu/cpufreq/policy6/scaling_cur_freq 2>/dev/null");
    out->cpu_freq = -1;
    if (cpu_raw && strlen(cpu_raw) > 0) {
        out->cpu_freq = atoi(cpu_raw);
        free(cpu_raw);
    } else {
        free(cpu_raw);
    }

    april_emit_event(APRIL_EVENT_METRIC_THERMAL,  out->thermal);
    april_emit_event(APRIL_EVENT_METRIC_BATTERY,  out->battery);
    april_emit_event(APRIL_EVENT_METRIC_CPUFREQ,  out->cpu_freq);

    return SENSEI_STATUS_OK;
}

void splinter_shutdown(void) {
    leo_shutdown();
    april_log("INFO", "Splinter Dojo: shutdown.");
}

BACKEND_TYPE splinter_get_backend(void) {
    return g_backend;
}
