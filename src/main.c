#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "daemon/capabilities_extra.h"

void run_full_scan() {
    printf("\n=== [D9 SECTOR: FULL SYSTEM INTERROGATION] ===\n");
    
    printf("\n[1] ACTIVE GHOST SERVICES:\n");
    system("service list | grep -E 'greezer|whetstone|security|daemon' | sed 's/^/  |- /'");

    printf("\n[2] LIVE BINDER TRAFFIC (Wiretap):\n");
    // Directly pull the Package stats - skipping the heavy dumpsys overhead
    system("dumpsys binder_calls_stats | grep 'Package:' | head -n 5 | sed 's/^/  |- /'");

    printf("\n[3] THE HOG LIST (Fast-Path):\n");
    // Skip the 'meminfo' service dump entirely and use 'ps' for raw RSS memory
    // This avoids the 'Broken Pipe' by using a lighter system call.
    system("ps -Ao RSS,NAME,PID | grep -E 'miui|xiaomi|daemon' | sort -rn | head -n 5 | awk '{print \"  |- \" $1 \"K: \" $2 \" (PID: \" $3 \")\"}'");

    printf("\n=== SCAN COMPLETE: PAYLOAD READY ===\n");
}

int main(int argc, char *argv[]) {
    char *mode = "doctor"; 
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) mode = argv[i+1];
    }
    detect_capabilities();
    capability_print_summary();
    if (strcmp(mode, "passive") == 0) run_full_scan();
    else capability_print_hints();
    return 0;
}
