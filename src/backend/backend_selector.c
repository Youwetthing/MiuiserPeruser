#include "backends/backend_common.h"
#include "backends/backend_april.h"
#include "backends/backend_rish.h"
#include "backends/backend_shizuku.h"
#include "backends/backend_adb.h"
#include "backends/backend_shizuku.h"
// sysport + fallback later

static backend_info_t g_active_info = { BACKEND_NONE, "none", false, false };
static backend_vtable_t g_active_vtable = { 0 };

const backend_info_t *backend_get_active_info(void) {
    return &g_active_info;
}

const backend_vtable_t *backend_get_active_vtable(void) {
    return (g_active_info.type == BACKEND_NONE) ? NULL : &g_active_vtable;
}

static int try_backend(const backend_info_t *info, const backend_vtable_t *vt) {
    if (!vt || !vt->init) return -1;
    if (vt->init() != 0) return -1;
    g_active_info = *info;
    g_active_vtable = *vt;
    return 0;
}

int backend_select_best(void) {
    // Order: April → Rish → Shizuku → ADB → (later: sysport → fallback)

    const backend_info_t april_info = { BACKEND_APRIL, "April", true, false };
    if (try_backend(&april_info, backend_april_vtable()) == 0) return 0;

    const backend_info_t rish_info = { BACKEND_RISH, "Rish", true, false };
    if (try_backend(&rish_info, backend_rish_vtable()) == 0) return 0;

    const backend_info_t shiz_info = { BACKEND_SHIZUKU, "Shizuku", true, false };
    if (try_backend(&shiz_info, backend_shizuku_vtable()) == 0) return 0;

    const backend_info_t adb_info = { BACKEND_ADB, "ADB", true, true };
    if (try_backend(&adb_info, backend_adb_vtable()) == 0) return 0;

    g_active_info.type = BACKEND_TERMUX_FALLBACK;
    g_active_info.name = "Termux fallback";
    g_active_info.privileged = false;
    g_active_info.via_network = false;
    return 0;
}
