#include "backends/backend_shizuku.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Placeholder binder FD
static int shizuku_fd = -1;

static int shizuku_init(void) {
    // TODO:
    //  - open binder driver
    //  - query Shizuku service
    //  - get service handle
    //  - perform handshake
    //  - verify permissions

    // For now, return failure so selector falls through
    return -1;
}

static int shizuku_shutdown(void) {
    if (shizuku_fd >= 0) {
        close(shizuku_fd);
        shizuku_fd = -1;
    }
    return 0;
}

static int shizuku_read_file(const char *path, char *buf, int buf_size) {
    // TODO:
    //  - send binder transaction to Shizuku
    //  - request file read
    //  - copy result into buf
    (void)path; (void)buf; (void)buf_size;
    return -1;
}

static int shizuku_run_command(const char *cmd, char *buf, int buf_size) {
    // TODO:
    //  - send binder transaction to Shizuku
    //  - request command execution
    //  - copy result into buf
    (void)cmd; (void)buf; (void)buf_size;
    return -1;
}

static const backend_vtable_t g_shizuku_vtable = {
    .init = shizuku_init,
    .shutdown = shizuku_shutdown,
    .read_file = shizuku_read_file,
    .run_command = shizuku_run_command
};

const backend_vtable_t *backend_shizuku_vtable(void) {
    return &g_shizuku_vtable;
}
