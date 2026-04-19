#include "rish_pipe.h"
#include <stdlib.h>

/*
 * Minimal stub implementations to satisfy the linker.
 * These do nothing but allow the daemon and detection engine to run.
 */

int rish_pipe_start(void) {
    /* no-op */
    return 0;
}

void rish_pipe_stop(void) {
    /* no-op */
}

char *rish_pipe_command(const char *cmd) {
    (void)cmd;
    /* return NULL to indicate "no output" */
    return NULL;
}
