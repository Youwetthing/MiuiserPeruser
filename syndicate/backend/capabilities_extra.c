#include <stdio.h>
#include "capabilities_extra.h"
#include "port_pathway.h"

/* Global capability state */
struct capabilities_state capabilities;

void detect_capabilities(void) {
    /* Reset everything */
    capabilities.adb = 0;
    capabilities.shizuku = 0;
    capabilities.proc_access = 0;
    capabilities.sys_access = 0;
    capabilities.miui_services = 0;

    capabilities.port_bridge_available = 0;
    capabilities.port_bridge_info_ok = 0;

    /* --- Port bridge detection --- */
    if (splinter_protocol_probe()) {
        capabilities.port_bridge_available = 1;

        if (splinter_protocol_basic_info()) {
            capabilities.port_bridge_info_ok = 1;
        }
    }

    /* You can add more detection logic here later */
}

/* ---------------- Summary Printer ---------------- */

void capability_print_summary(void) {
    printf("Summary:\n");
    printf("  ADB:                 %d\n", capabilities.adb);
    printf("  Shizuku:             %d\n", capabilities.shizuku);
    printf("  Proc access:         %d\n", capabilities.proc_access);
    printf("  Sys access:          %d\n", capabilities.sys_access);
    printf("  MIUI services:       %d\n", capabilities.miui_services);
    printf("  Port bridge avail:   %d\n", capabilities.port_bridge_available);
    printf("  Port bridge info:    %d\n", capabilities.port_bridge_info_ok);
}

/* ---------------- Hint Printer ---------------- */

void capability_print_hints(void) {
    if (!capabilities.port_bridge_available) {
        printf("Hint: No Sensei port bridge detected on 127.0.0.1:%d\n",
               SENSEI_PORT_BRIDGE);
    } else if (!capabilities.port_bridge_info_ok) {
        printf("Hint: Bridge responded but did not return info.\n");
    } else {
        printf("Hint: Port bridge fully operational.\n");
    }
}
