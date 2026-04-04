#include "backends/backend_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define SYS_PORT_LISTEN_ADDR "127.0.0.1"
#define SYS_PORT_LISTEN_PORT 5959
#define BUF_SIZE 4096

/* --- utilities omitted for brevity (unchanged) --- */
/* KEEP ALL YOUR EXISTING CODE ABOVE THIS COMMENT */
/* DO NOT DELETE ANYTHING EXCEPT THE OLD VTABLE */

static int sysport_init(void) {
    /* keep your existing server code */
    return 0;
}

static int sysport_shutdown(void) {
    return 0;
}

/* --- NEW unified backend API --- */

static int sysport_read_thermal(int *out) {
    if (!out) return -1;
    *out = 42000; /* stub */
    return 0;
}

static int sysport_read_battery(int *out) {
    if (!out) return -1;
    *out = 85; /* stub */
    return 0;
}

static int sysport_read_cpu_freq(int *out) {
    if (!out) return -1;
    *out = 1785600; /* stub */
    return 0;
}

/* --- NEW vtable --- */

static const backend_vtable_t g_sysport_vtable = {
    .init          = sysport_init,
    .shutdown      = sysport_shutdown,
    .read_thermal  = sysport_read_thermal,
    .read_battery  = sysport_read_battery,
    .read_cpu_freq = sysport_read_cpu_freq
};

const backend_vtable_t *backend_sysport_vtable(void) {
    return &g_sysport_vtable;
}
