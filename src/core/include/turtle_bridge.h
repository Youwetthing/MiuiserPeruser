#ifndef TURTLE_BRIDGE_H
#define TURTLE_BRIDGE_H

typedef struct {
    float anomaly_score;
    char report[256];
} ScanResult;

// The Squad's Specialities
ScanResult scan_leo_strategy();   // Process behavior
ScanResult scan_don_integrity();  // System/Memory integrity
ScanResult scan_mikey_miui();     // HyperOS/MIUI Bloat tracking
ScanResult scan_raph_network();   // Ghost Protocol/Port bridge

#endif
