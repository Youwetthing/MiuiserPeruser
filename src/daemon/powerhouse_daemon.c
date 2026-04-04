#include <stdio.h>
#include <unistd.h>
#include "rocksteady_intel.h"
#include "splinter_intel.h"

extern void ingest_rocksteady_intel();
extern void ingest_krangd_intel();
extern void ingest_splinterd_intel();
extern void log_brain_event(const char* event);
extern splinter_packet_t current_master_intel;

int main() {
    printf("[BRAIN] Syndicate Neural Link established.\n");
    log_brain_event("Neural Loop Started: Sensei-Class.");

    while (1) {
        ingest_rocksteady_intel();
        ingest_krangd_intel();
        ingest_splinterd_intel();

        // STRATEGIC LOGIC
        if (current_master_intel.harmony_score > 80) {
            printf("[MASTER] Chaos detected! Harmony Score: %d. Forcing cooldown.\n", 
                   current_master_intel.harmony_score);
            log_brain_event("Strategic Override: High Chaos.");
            // Action logic here...
        }

        sleep(10);
    }
    return 0;
}
