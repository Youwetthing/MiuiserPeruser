#include "backends/backend_common.h"
#include "backends/backend_adb.h"

#include <string.h>

/* ADB backend — stub implementation */

static int adb_init(void) {
    return -1; /* stub */
}

static int adb_shutdown(void) {
    return 0;
}

/* --- NEW unified backend API --- */

static int adb_read_thermal(int *out) {
    if (!out) return -1;
    *out = 44000; /* stub */
    return 0;
}

static int adb_read_battery(int *out) {
    if (!out) return -1;
    *out = 79; /* stub */
    return 0;
}

static int adb_read_cpu_freq(int *out) {
    if (!out) return -1;
    *out = 1728000; /* stub */
    return 0;
}

/* --- NEW vtable --- */

static const backend_vtable_t g_adb_vtable = {
    .init          = adb_init,
    .shutdown      = adb_shutdown,
    .read_thermal  = adb_read_thermal,
    .read_battery  = adb_read_battery,
    .read_cpu_freq = adb_read_cpu_freq
};

const backend_vtable_t *backend_adb_vtable(void) {
    return &g_adb_vtable;
}
