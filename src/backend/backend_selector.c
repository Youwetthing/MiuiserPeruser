#include "backends/backend_common.h"

#include <string.h>
#include <stdio.h>

// Backend selector — decides which backend to activate.
// Currently stubbed until real detection logic is implemented.

static backend_type_t g_active_backend = BACKEND_NONE;

int backend_select_best(void) {
    // TODO: implement real detection logic
    g_active_backend = BACKEND_NONE;
    return 0;
}

const backend_info_t *backend_get_active_info(void) {
    static backend_info_t info = {
        .type        = BACKEND_NONE,
        .name        = "none",
        .privileged  = false,
        .via_network = false
    };
    return &info;
}

const backend_vtable_t *backend_get_active_vtable(void) {
    // No active backend yet
    return NULL;
}
