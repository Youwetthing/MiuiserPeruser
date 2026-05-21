#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "ipc_globals.h"

int main() {
    printf("[SPLINTER] Splinterd initialising...\n");
    printf("[SPLINTER] Monitoring Sewer at: %s\n", SEWER_SOCKET);
    
    while (g_running) {
        // Reduced heartbeat to 1 second to save CPU
        usleep(1000000); 
    }
    return 0;
}
