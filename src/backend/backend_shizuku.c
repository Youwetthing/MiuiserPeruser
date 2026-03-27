#include "backends/backend_common.h"
#include "backends/backend_shizuku.h"

#include <string.h>

/* Shizuku backend — stub implementation */

static int shizuku_init(void) {
    return -1; /* stub */
}

static int shizuku_shutdown(void) {
    return 0;
}

/* --- NEW unified backend API --- */

static int shizuku_read_thermal(int *out) {
    if (!out) return -1;
    *out = 41000; /* stub */
    return 0;
}

static int shizuku_read_battery(int *out) {
    if (!out) return -1;
    *out = 82; /* stub */
    return 0;
}

static int shizuku_read_cpu_freq(int *out) {
    if (!out) return -1;
    *out = 1850000; /* stub */
    return 0;
}

/* --- NEW vtable --- */

static const backend_vtable_t g_shizuku_vtable = {
    .init          = shizuku_init,
    .shutdown      = shizuku_shutdown,
    .read_thermal  = shizuku_read_thermal,
    .read_battery  = shizuku_read_battery,
    .read_cpu_freq = shizuku_read_cpu_freq
};

const backend_vtable_t *backend_shizuku_vtable(void) {
    return &g_shizuku_vtable;
}
