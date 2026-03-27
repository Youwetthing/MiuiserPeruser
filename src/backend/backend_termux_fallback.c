#include "backends/backend_common.h"
#include "backends/backend_termux_fallback.h"

#include <string.h>

/* Termux fallback backend — stub implementation */

static int termux_init(void) {
    return 0; /* fallback always available */
}

static int termux_shutdown(void) {
    return 0;
}

/* --- NEW unified backend API --- */

static int termux_read_thermal(int *out) {
    if (!out) return -1;
    *out = 45000; /* stub */
    return 0;
}

static int termux_read_battery(int *out) {
    if (!out) return -1;
    *out = 78; /* stub */
    return 0;
}

static int termux_read_cpu_freq(int *out) {
    if (!out) return -1;
    *out = 1600000; /* stub */
    return 0;
}

/* --- NEW vtable --- */

static const backend_vtable_t g_termux_vtable = {
    .init          = termux_init,
    .shutdown      = termux_shutdown,
    .read_thermal  = termux_read_thermal,
    .read_battery  = termux_read_battery,
    .read_cpu_freq = termux_read_cpu_freq
};

const backend_vtable_t *backend_termux_fallback_vtable(void) {
    return &g_termux_vtable;
}
