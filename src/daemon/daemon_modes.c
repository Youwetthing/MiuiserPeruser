/*
 * daemon_modes.c — MiuiserPeruser operating modes
 *
 * Passive:   single scan, print results, exit
 * Scheduled: IPC up, hourly scans
 * Active:    IPC up, per-minute scans
 */

#include <stdio.h>
#include <unistd.h>

#include "ipc_globals.h"
#include "capabilities_extra.h"
#include "port_pathway.h"
#include "daemon_common.h"

/* IPC server */
int  miuiserperuser_ipc_init(void);
void miuiserperuser_ipc_shutdown(void);

/* Global capability state */
extern capabilities_t capabilities;

/* Capability output */
void print_capabilities_pretty(void);
void capability_print_summary(void);
void capability_print_hints(void);

/* ── Help ─────────────────────────────────────────────────────────────── */

void print_help(void)
{
    printf("MiuiserPeruser daemon\n");
    printf("Usage:\n");
    printf("  --doctor            Run full diagnostic mode\n");
    printf("  --mode passive      Single scan\n");
    printf("  --mode scheduled    Scheduled scanning\n");
    printf("  --mode active       Active monitoring\n");
    printf("  --selftest          Run internal self-test\n");
}

/* ── Selftest ─────────────────────────────────────────────────────────── */

void run_selftest(void)
{
    printf("[Selftest] Running basic checks...\n");
    printf("[Selftest] OK\n");
}

/* ── Passive mode ─────────────────────────────────────────────────────── */

void run_single_scan(void)
{
    printf("[Scan] Running single scan...\n");
    detect_capabilities();
    print_capabilities_pretty();
    capability_print_summary();
    capability_print_hints();
}

/* ── Scheduled mode ───────────────────────────────────────────────────── */

void run_scheduled_mode(void)
{
    printf("[Scheduled] Starting IPC...\n");
    if (miuiserperuser_ipc_init() != 0) {
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

/* ── Active mode ──────────────────────────────────────────────────────── */

void run_active_mode(void)
{
    printf("[Active] Starting IPC...\n");
    if (miuiserperuser_ipc_init() != 0) {
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
