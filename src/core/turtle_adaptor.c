#include "turtle_bridge.h"
#include <stdio.h>

extern int raph_network_scan();
extern int casey_input_audit();
extern int splinter_status_check(); // New
extern int leatherhead_io_audit();  // New

ScanResult scan_system_full() {
    ScanResult r;
    int net = raph_network_scan();
    int inp = casey_input_audit();
    int sup = splinter_status_check();
    int iod = leatherhead_io_audit();

    r.anomaly_score = (net * 0.2) + (inp * 0.5) + (iod * 0.3);
    snprintf(r.report, 256, "N:%d|I:%d|S:%d|L:%d", net, inp, sup, iod);
    return r;
}
