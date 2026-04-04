#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "capabilities_extra.h"
#include "port_pathway.h"
#include "splinter_protocol.h"

struct capabilities_state capabilities;

static int check_shizuku(void) {
    if (access("/data/local/tmp/shizuku_starter", F_OK) == 0) return 1;
    if (getenv("SHIZUKU_TOKEN") != NULL) return 1;
    FILE *fp = fopen("/proc/net/unix", "r");
    if (fp) {
        char line[512];
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, "shizuku")) { fclose(fp); return 1; }
        }
        fclose(fp);
    }
    return 0;
}

static int check_adb(void) {
    // 1. Check Global Settings (Reliable under rish/shell)
    FILE *fp = popen("settings get global adb_enabled 2>/dev/null", "r");
    if (fp) {
        char buf[16];
        if (fgets(buf, sizeof(buf), fp) && buf[0] == '1') {
            pclose(fp);
            return 1;
        }
        pclose(fp);
    }
    // 2. Fallback to binary check
    if (system("which adb > /dev/null 2>&1") == 0) return 1;
    return 0;
}

void detect_capabilities(void) {
    capabilities.adb = check_adb();
    capabilities.shizuku = check_shizuku();
    capabilities.proc_access = 1; 
    capabilities.sys_access = 1;
    capabilities.miui_services = 1;
    capabilities.port_bridge_available = splinter_protocol_probe() ? 1 : 0;
    capabilities.port_bridge_info_ok = (capabilities.port_bridge_available && splinter_protocol_basic_info()) ? 1 : 0;
}

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

void capability_print_hints(void) {
    if (capabilities.adb && capabilities.shizuku) printf("Hint: [TRIPLE CROWN] Syndicate is fully operational.\n");
    else printf("Hint: Check developer options if flags are missing.\n");
}
