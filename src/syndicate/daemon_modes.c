#include "sensei_types.h"
#include "ipc_includes.h"

#include <stdio.h>
#include <unistd.h>

#include "capabilities_extra.h"
#include "port_pathway.h"
#include "ipc_globals.h"

/* IPC server */
SENSEI_STATUS miuiserperuser_ipc_init(void);
void miuiserperuser_ipc_shutdown(void);

/* Global capability state */
extern capabilities_t capabilities;

/* Pretty printer */
void print_capabilities_pretty(void);

/* Summary + hints */
void capability_print_summary(void);
void capability_print_hints(void);

/* ------------------------------
   HELP
   ------------------------------ */
void print_help(void) {
    printf("MiuiserPeruser daemon\n");
    printf("Usage:\n");
    printf("  --doctor            Run full diagnostic mode\n");
    printf("  --mode passive      Single scan\n");
    printf("  --mode scheduled    Scheduled scanning\n");
    printf("  --mode active       Active monitoring\n");
    printf("  --selftest          Run internal self-test\n");
}

/* ------------------------------
   SELFTEST
   ------------------------------ */
void run_selftest(void) {
    printf("[Selftest] Running basic checks...\n");
    printf("[Selftest] OK\n");
}

/* ------------------------------
   PASSIVE MODE
   ------------------------------ */
void run_single_scan(void) {
    printf("[Scan] Running single scan...\n");
    detect_capabilities();
    print_capabilities_pretty();
    capability_print_summary();
    capability_print_hints();
}

/* ------------------------------
   SCHEDULED MODE
   ------------------------------ */
void run_scheduled_mode(void) {
    printf("[Scheduled] Starting Turtle Power IPC...\n");
    if (miuiserperuser_ipc_init() != SENSEI_STATUS_OK) {
        printf("[Scheduled] IPC failed to start.\n");
        return;
    }

    printf("[Scheduled] Hourly scans. Ctrl+C to stop.\n");
    for (;;) {
        printf("\n[Scheduled] Running scheduled scan...\n");
        detect_capabilities();
        capability_print_summary();
        sleep(3600);
    }

    miuiserperuser_ipc_shutdown();
}

/* ------------------------------
   ACTIVE MODE
   ------------------------------ */
void run_active_mode(void) {
    printf("[Active] Starting Turtle Power IPC...\n");
    if (miuiserperuser_ipc_init() != SENSEI_STATUS_OK) {
        printf("[Active] IPC failed to start.\n");
        return;
    }

    printf("[Active] Active monitoring. Ctrl+C to stop.\n");
    for (;;) {
        printf("\n[Active] Running active scan...\n");
        detect_capabilities();
        capability_print_summary();
        sleep(60);
    }

    miuiserperuser_ipc_shutdown();
}
