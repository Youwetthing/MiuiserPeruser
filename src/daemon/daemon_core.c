#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <pthread.h>
#include "daemon_core.h"
#include "ipc_globals.h"
#include "daemon_common.h"

/* ── Globals (definitions owned here, declared extern in ipc_globals.h) ─ */
/* NOTE: ipc_globals.c also provides these for the full IPC build.         *
 * Daemon executables that compile only daemon_core.c (not ipc_globals.c)  *
 * use these definitions.                                                   */
/* DO NOT duplicate: ipc_globals.c is the single source of truth.          *
 * daemon_core.c only pulls them in via the extern from ipc_globals.h.     */

/* ── Per-daemon state ─────────────────────────────────────────────────── */
static char g_daemon_name[64] = "daemon";

static void handle_stop(int sig)
{
    (void)sig;
    g_running = false;
    _exit(0);   /* async-signal-safe; unblocks for(;;) loops in simple daemons */
}

/* ── daemon_core_init ─────────────────────────────────────────────────── */

int daemon_core_init(const char *name)
{
    if (name && *name)
        snprintf(g_daemon_name, sizeof(g_daemon_name), "%s", name);

    signal(SIGINT,  handle_stop);
    signal(SIGTERM, handle_stop);
    signal(SIGPIPE, SIG_IGN);

    daemon_log_info("starting");
    return 1;
}

void daemon_core_shutdown(void)
{
    daemon_log_info("shutdown complete");
}

/* ── Logging ──────────────────────────────────────────────────────────── */

void daemon_log_info(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stdout, "[%s][INFO] ", g_daemon_name);
    vfprintf(stdout, fmt, ap);
    fputc('\n', stdout);
    fflush(stdout);
    va_end(ap);
}

void daemon_log_warn(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[%s][WARN] ", g_daemon_name);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    fflush(stderr);
    va_end(ap);
}

void daemon_log_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[%s][ERROR] ", g_daemon_name);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    fflush(stderr);
    va_end(ap);
}

/* ── Main-process IPC loop (used by miuiserperuser host binary) ───────── */

static bool g_console_mode = false;

static void miuiserperuser_stop_signal(int sig)
{
    (void)sig;
    g_running = false;
}

int miuiserperuser_main_loop(bool console_mode)
{
    g_console_mode = console_mode;
    g_running = true;

    signal(SIGINT,  miuiserperuser_stop_signal);
    signal(SIGTERM, miuiserperuser_stop_signal);
    signal(SIGPIPE, SIG_IGN);

    fprintf(stdout, "[CORE] MiuiserPeruser daemon starting\n");

    if (miuiserperuser_ipc_init() != 0)
        fprintf(stderr, "[CORE] IPC init failed — continuing without IPC\n");

    fprintf(stdout, "[CORE] scan loop running\n");

    while (g_running) {
        for (int i = 0; i < 5 && g_running; i++)
            sleep(1);
    }

    fprintf(stdout, "[CORE] shutting down\n");
    miuiserperuser_ipc_shutdown();
    return 0;
}

/* ── Weak IPC stubs ───────────────────────────────────────────────────── *
 * Individual daemon binaries do not compile ipc.c.  These weak           *
 * definitions satisfy the linker; the real implementations in ipc.c      *
 * override them automatically when the full host binary is built.        *
 * ─────────────────────────────────────────────────────────────────────── */
__attribute__((weak)) int  miuiserperuser_ipc_init(void)     { return 0; }
__attribute__((weak)) void miuiserperuser_ipc_shutdown(void) { }
