#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "syndicate_db.h"
#include "turtle_bridge.h"

void broadcast(const char* turtle, const char* msg, int level) {
    const char* colors[] = {"\033[1;32m", "\033[1;33m", "\033[1;31m"}; // Green, Yellow, Red
    printf("%s[LIVE FEED][%s]\033[0m %s\n", colors[level], turtle, msg);
    syndicate_db_log(turtle, (level == 2 ? "CRITICAL" : "INFO"), msg);
}

int main(void) {
    syndicate_db_init("/data/data/com.termux/files/home/MiuiserPeruser/data/syndicate.db");
    
    printf("\033[2J\033[H\033[1;35m--- SYNDICATE NEWS NETWORK: SUPERHERO MODE ACTIVE ---\033[0m\n");

    while(1) {
        // LEO: Check for out-of-ordinary processes
        ScanResult leo = scan_leo_strategy();
        broadcast("LEO", leo.report, (leo.anomaly_score > 0.7 ? 2 : 0));

        // DONNIE: Integrity Check
        ScanResult don = scan_don_integrity();
        broadcast("DON", don.report, (don.anomaly_score > 0.5 ? 1 : 0));

        // MIKEY: MIUI/HyperOS specific goss... I mean, data
        ScanResult mikey = scan_mikey_miui();
        broadcast("MIKEY", mikey.report, 0);

        printf("\n\033[1;30m[Waiting for next Ghost Protocol update...]\033[0m\n");
        sleep(3);
    }
    return 0;
}
