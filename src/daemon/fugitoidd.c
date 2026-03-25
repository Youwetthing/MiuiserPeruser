// fugitoidd.c — Memory / ZRAM / PSI daemon
// Backend TMNT daemon: Fugitoid
// Dashboard persona: Neil Daemon

#include "krang.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void report(const char *key, const char *value) {
    char msg[256];
    snprintf(msg, sizeof(msg), "%s=%s", key, value);
    krang_send_command(msg);
}

static char* run(const char *cmd) {
    return krang_send_command(cmd); // malloc'd
}

static void check_meminfo(void) {
    char *r = run("cat /proc/meminfo");
    if (!r) return;
    report("MEMINFO", r);
    free(r);
}

static void check_zram(void) {
    char *r = run("cat /sys/block/zram0/mem_used_total 2>/dev/null");
    if (!r) return;
    report("ZRAM_USED", r);
    free(r);
}

static void check_swap(void) {
    char *r = run("cat /proc/swaps");
    if (!r) return;
    report("SWAP", r);
    free(r);
}

static void check_psi(void) {
    char *r = run("cat /proc/pressure/memory 2>/dev/null");
    if (!r) return;
    report("PSI_MEMORY", r);
    free(r);
}

static void check_lowmem_killer(void) {
    char *r = run("dmesg | grep -i lowmemorykiller | tail -n 5");
    if (!r) return;
    report("LOWMEM_KILLER", r);
    free(r);
}

static void run_checks(void) {
    check_meminfo();
    check_zram();
    check_swap();
    check_psi();
    check_lowmem_killer();
}

int main(void) {
    while (krang_connect() != 0)
        sleep(2);

    krang_send_command("FUGITOID=online");

    for (;;) {
        run_checks();
        sleep(50);
    }

    return 0;
}
