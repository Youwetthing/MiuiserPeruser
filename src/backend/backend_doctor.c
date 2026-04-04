#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/backends/backend_common.h"

void trigger_panic() {
    printf("\n🚨 [SYSTEM ALARM: NORTHWOOD SECTOR] 🚨\n");
    printf("📢 MOUSER SHOUTS: \"There's a Hurr in me Sewer!\"\n");
    printf("⚡ [EMERGENCY] Flushing the Abbey St. Spine... (Bus 41)\n");
    
    // Execute the Reset immediately
    system("./ignite.sh"); 
}

void mouser_scan() {
    printf("[MOUSER] Sniffing the pipes for rogue MIUI 'Hurrs'...\n");
    
    // Checks if the MIUI daemon is trying to creep back in
    int detection = system("pgrep -f 'com.miui.daemon' > /dev/null");
    
    if (detection == 0) {
        trigger_panic();
    } else {
        printf("✅ [MOUSER] Sewer is clear. No Hurrs detected in D9.\n");
    }
}

int main(int argc, char *argv[]) {
    printf("\n🐢 [ MOUSER-D9 | TECHNODROME SENTRY ] 🐢\n");
    
    if (argc > 1) {
        if (strcmp(argv[1], "--33") == 0) {
            mouser_scan();
        } else if (strcmp(argv[1], "--16") == 0) {
            printf("[MOUSER] Beaumont Run: Purging old log debris...\n");
            system("mkdir -p backups && mv *.log backups/ 2>/dev/null");
        } else if (strcmp(argv[1], "--41") == 0 || strcmp(argv[1], "--panic") == 0) {
            trigger_panic();
        } else if (strcmp(argv[1], "--sentry") == 0) {
            printf("Status: OPERATIONAL | Sector: NORTHWOOD\n");
        }
    } else {
        printf("Usage: mouser [--16 (Purge) | --33 (Scan) | --41 (Reset) | --panic]\n");
    }
    return 0;
}
