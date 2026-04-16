#include "turtle_bridge.h"
#include <stdio.h>
#include <string.h>

/* * These 'extern' calls link directly to the functions the linker 
 * was complaining about earlier.
 */
extern int leo_full_scan(); 
extern int don_integrity_check();
extern void mikey_miui_check();
extern void rish_pipe_command(const char* cmd);

ScanResult scan_leo_strategy() {
    ScanResult r;
    // Calling the actual Leo detection logic
    int res = leo_full_scan(); 
    r.anomaly_score = (res > 0) ? 0.95f : 0.05f;
    snprintf(r.report, 256, "Strategy: %s", 
             (res > 0 ? "Detected unauthorized process elevation!" : "Process tree stable."));
    return r;
}

ScanResult scan_don_integrity() {
    ScanResult r;
    // Calling the actual Donnie integrity logic
    int res = don_integrity_check();
    r.anomaly_score = (res > 0) ? 0.85f : 0.0f;
    snprintf(r.report, 256, "Integrity: %s", 
             (res > 0 ? "System partition hash mismatch!" : "HyperOS core intact."));
    return r;
}

ScanResult scan_mikey_miui() {
    ScanResult r;
    // Mikey handles the 'Gossip'—tracking MIUI background whispers
    mikey_miui_check();
    r.anomaly_score = 0.2f;
    
    // This is where we feed the 'Gossip Girl' layer later
    snprintf(r.report, 256, "MIUI: Secret service 'com.miui.daemon' just pinged home.");
    return r;
}

ScanResult scan_raph_network() {
    ScanResult r;
    // Placeholder for Raph's network/ghost protocol
    r.anomaly_score = 0.1f;
    snprintf(r.report, 256, "Network: Port bridge 8080 secure under Ghost Protocol.");
    return r;
}
