#include <stdio.h>
#include "capabilities_extra.h"

/* Global capability state */
extern struct capabilities_state capabilities;

void print_capabilities_pretty(void) {
    printf("[Capabilities]\n");

    printf("  ADB:                 %d\n", capabilities.adb);
    printf("  Shizuku:             %d\n", capabilities.shizuku);
    printf("  Proc access:         %d\n", capabilities.proc_access);
    printf("  Sys access:          %d\n", capabilities.sys_access);
    printf("  MIUI services:       %d\n", capabilities.miui_services);

    printf("  Port bridge avail:   %d\n", capabilities.port_bridge_available);
    printf("  Port bridge info:    %d\n", capabilities.port_bridge_info_ok);

    printf("\n");
}
