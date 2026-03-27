#include "backends/backend_common.h"
#include "backends/backend_rish.h"

#include <string.h>

/* RISH backend — stub implementation */

static int rish_init(void) {
    return -1; /* stub */
}

static int rish_shutdown(void) {
    return 0;
}

/* --- NEW unified backend API --- */

static int rish_read_thermal(int *out) {
    if (!out) return -1;
    *out = 43000; /* stub */
    return 0;
}

static int rish_read_battery(int *out) {
    if (!out) return -1;
    *out = 83; /* stub */
    return 0;
}

static int rish_read_cpu_freq(int *out) {
    if (!out) return -1;
    *out = 1760000; /* stub */
    return 0;
}

/* --- NEW vtable --- */

static const backend_vtable_t g_rish_vtable = {
    .init          = rish_init,
    .shutdown      = rish_shutdown,
    .read_thermal  = rish_read_thermal,
    .read_battery  = rish_read_battery,
    .read_cpu_freq = rish_read_cpu_freq
};

const backend_vtable_t *backend_rish_vtable(void) {
    return &g_rish_vtable;
}
