#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include "modules/modules.h"

static volatile int running = 1;

void handle_sig(int sig) {
    running = 0;
}

void scan_radio();
void scan_environment();

int main() {
    signal(SIGINT, handle_sig);
    signal(SIGTERM, handle_sig);

    printf("[RAHZERD] daemon online\n");

    while (running) {

        printf("\n[RAHZERD] === cycle start ===\n");

        scan_radio();
        scan_environment();

        printf("[RAHZERD] === cycle end ===\n");

        sleep(10);
    }

    printf("[RAHZERD] shutdown complete\n");
    return 0;
}
