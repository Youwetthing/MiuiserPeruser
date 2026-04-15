#include "daemon_common.h"
#include <string.h>

int miui_flag_restricted = 0;
int thermal_state = 0;

__attribute__((weak))
int process_running(const char *name) {
    if (name && strcmp(name, "tigerclaw") == 0) return 1;
    return 0;
}

__attribute__((weak))
int krang_connect(void) { return 0; }

__attribute__((weak))
int krang_send_command(const char *cmd) { (void)cmd; return 0; }

__attribute__((weak))
const char *device_get_property(const char *key) {
    (void)key;
    return "";
}
