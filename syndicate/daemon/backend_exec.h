/*
 * backend_exec.h — Privileged shell execution backend
 *
 * Probes once at init for the best available privileged shell:
 *
 *   1. rish  — ~/.shizuku/rish (Shizuku privileged shell, preferred)
 *   2. ADB   — adb shell (USB/TCP bridge)
 *   3. Direct— plain popen(cmd) — works for getprop, getenforce,
 *              world-readable /proc and /sys paths
 *
 * All commands that need system_server or root context (dumpsys, logcat,
 * pm, am …) must go through bexec() to actually receive output on MIUI.
 *
 * Usage:
 *   bexec_init();             // call once in main() before the poll loop
 *   char *out = bexec("dumpsys power");
 *   if (out) { ... free(out); }
 *
 *   // For sysfs/proc paths that may be root-restricted:
 *   char *temp = bexec_read_file("/sys/class/thermal/thermal_zone0/temp");
 */

#ifndef DAEMON_BACKEND_EXEC_H
#define DAEMON_BACKEND_EXEC_H

#include <stddef.h>

/* ── Backend kinds ────────────────────────────────────────────────────── */

typedef enum {
    BEXEC_RISH   = 0,   /* ~/.shizuku/rish -c '…' or rish in PATH     */
    BEXEC_ADB    = 1,   /* adb shell …                                  */
    BEXEC_DIRECT = 2,   /* plain popen(cmd) — no privilege elevation    */
} bexec_backend_t;

/* ── Lifecycle ────────────────────────────────────────────────────────── */

/*
 * bexec_init — probe for the best backend and cache the result.
 * Safe to call multiple times; only probes on the first call.
 * Prints a one-line banner to stdout indicating which backend was found.
 */
void bexec_init(void);

/* Return the currently selected backend (DIRECT if init not yet called). */
bexec_backend_t bexec_backend(void);

/* Human-readable name of the active backend ("rish", "adb", "direct"). */
const char     *bexec_backend_name(void);

/* ── Command execution ────────────────────────────────────────────────── */

/*
 * bexec — run cmd through the active privileged backend.
 *
 * Returns a malloc'd NUL-terminated string containing the command's
 * stdout, or NULL if the command produced no output or failed.
 * Caller must free() the result.
 *
 * Default output buffer: 65536 bytes.
 */
char *bexec(const char *cmd);

/*
 * bexec_n — same as bexec with an explicit output size cap.
 * Useful for commands like "dumpsys sensorservice" that produce many KB.
 */
char *bexec_n(const char *cmd, size_t maxout);

/* ── Sysfs / procfs helper ────────────────────────────────────────────── */

/*
 * bexec_read_file — read a sysfs/procfs file.
 *
 * Tries fopen() first (fast, works for world-readable paths).
 * Falls back to bexec("cat <path>") for root-restricted nodes.
 *
 * Returns malloc'd content or NULL.  Caller must free().
 */
char *bexec_read_file(const char *path);

#endif /* DAEMON_BACKEND_EXEC_H */
