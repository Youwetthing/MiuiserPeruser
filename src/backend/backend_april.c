#include "backends/backend_april.h"
#include <string.h>

// TODO: wire to real April bridge

static int april_init(void) {
    // Detect April presence, bind to service, etc.
    // Return 0 on success, non-zero on failure.
    return -1; // stub: disabled until wired
}

static int april_shutdown(void) {
    return 0;
}

static int april_read_file(const char *path, char *buf, int buf_size) {
    (void)path; (void)buf; (void)buf_size;
    return -1; // stub
}

static int april_run_command(const char *cmd, char *buf, int buf_size) {
    (void)cmd; (void)buf; (void)buf_size;
    return -1; // stub
}

static const backend_vtable_t g_april_vtable = {
    .init = april_init,
    .shutdown = april_shutdown,
    .read_file = april_read_file,
    .run_command = april_run_command
};

const backend_vtable_t *backend_april_vtable(void) {
    return &g_april_vtable;
}
