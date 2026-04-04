#include "../../include/backends/backend_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int rish_init() {
    printf("[SHREDDER] Checking Shizuku Mask...\n");
    // Check if rish (Shizuku CLI) is accessible
    if (system("command -v rish > /dev/null 2>&1") == 0) {
        printf("[SHREDDER] Mask Verified. Privileged access granted.\n");
        return 0;
    } else {
        printf("[SHREDDER] WARNING: rish not found in PATH. Falling back to Foot Soldier (ADB).\n");
        return -1;
    }
}

int rish_send(const char* cmd) {
    char full_cmd[512];
    // This wraps your command in the Shizuku 'rish' caller
    snprintf(full_cmd, sizeof(full_cmd), "rish -c '%s'", cmd);
    return system(full_cmd);
}

const backend_vtable_t backend_rish_vtable = {
    .name = "SHREDDER_RISH",
    .init = rish_init,
    .send_cmd = rish_send
};
