#ifndef MIUISERPERUSER_DAEMON_CORE_H
#define MIUISERPERUSER_DAEMON_CORE_H

#include <stdbool.h>

/* ── Per-daemon lifecycle ─────────────────────────────────────────────── *
 * Call daemon_core_init() at the top of main().  Returns 1 on success,   *
 * 0 on failure.  Sets up signal handlers (SIGINT/SIGTERM → clean exit)   *
 * and prints a startup banner for the given daemon name.                  *
 * ─────────────────────────────────────────────────────────────────────── */
bool daemon_core_init(const char *name);
void daemon_core_shutdown(void);

/* ── Logging helpers (printf-style, prepend [name][LEVEL]) ───────────── */
void daemon_log_info (const char *fmt, ...);
void daemon_log_warn (const char *fmt, ...);
void daemon_log_error(const char *fmt, ...);

/* ── Main-process IPC API (used by the miuiserperuser host binary only) ─ */
int  miuiserperuser_ipc_init    (void);
void miuiserperuser_ipc_shutdown(void);
int  miuiserperuser_main_loop   (bool console_mode);

#endif /* MIUISERPERUSER_DAEMON_CORE_H */
