#include "fugitoid_log.h"
#include "fugitoid_log.h"
#include "capabilities.h"
/*
 * MiuiserPeruser – Daemon entry point
 */

#include "daemon_core.h"
#include "service.h"
#include <stdio.h>
#include <string.h>

void print_usage(const char *prog) {
    printf("MiuiserPeruser Daemon\n");
    printf("Usage: %s [command]\n\n", prog);
    printf("Commands:\n");
    printf("  --install    Install as service (not used on Termux)\n");
    printf("  --uninstall  Uninstall service\n");
    printf("  --console    Run in console mode (debug)\n");
    printf("  --help       Show this help\n");
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        if (strcmp(argv[1], "--console") == 0) {
            return miuiserperuser_main_loop(true);
        } else if (strcmp(argv[1], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            printf("Unknown command: %s\n", argv[1]);
            print_usage(argv[0]);
            return 1;
        }
    }
    /* No arguments – run as daemon */
    return miuiserperuser_service_start();
}
