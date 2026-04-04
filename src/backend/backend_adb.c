#include "../../include/backends/backend_common.h"
#include <stdio.h>

int adb_init() { printf("[ADB] Connected to Foot Clan.\n"); return 0; }
int adb_send(const char* c) { return 0; }

const backend_vtable_t backend_adb_vtable = {
    .name = "ADB",
    .init = adb_init,
    .send_cmd = adb_send
};
