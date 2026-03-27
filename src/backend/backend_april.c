#include "backends/backend_common.h"
#include "backends/backend_april.h"

#include <string.h>

/* April backend — stub implementation */

static int april_init(void) {
    return -1; /* stub */
}

static int april_shutdown(void) {
    return 0;
}

/* --- NEW unified backend API --- */

static int april_read_thermal(int *out) {
    if (!out) return -1;
    *out = 43000; /* stub */
    return 0;
}

static int april_read_battery(int *out) {
    if (!out) return -1;
    *out = 80; /* stub */
    return 0;
}

static int april_read_cpu_freq(int *out) {
    if (!out) return -1;
    *out = 1900000; /* stub */
    return 0;
}

/* --- NEW vtable --- */

static const backend_vtable_t g_april_vtable = {
    .init          = april_init,
    .shutdown      = april_shutdown,
    .read_thermal  = april_read_thermal,
    .read_battery  = april_read_battery,
    .read_cpu_freq = april_read_cpu_freq
};

const backend_vtable_t *backend_april_vtable(void) {
    return &g_april_vtable;
}
