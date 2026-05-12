/*
 * capabilities_pretty.c — formatted capability output
 */

#include <stdio.h>
#include "capabilities_extra.h"

void print_capabilities_pretty(void)
{
    printf("[Capabilities]\n");
    printf("  ADB:                 %d\n", capabilities.adb);
    printf("  Shizuku:             %d\n", capabilities.shizuku);
    printf("  Shizuku script:      %d\n", capabilities.shizuku_script);
    printf("  Rish:                %d\n", capabilities.rish);
    printf("  Port bridge avail:   %d\n", capabilities.port_bridge_available);
    printf("  Port bridge info:    %d\n", capabilities.port_bridge_info_ok);
    printf("\n");
}
