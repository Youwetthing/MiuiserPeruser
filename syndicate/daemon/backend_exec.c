/*
 * backend_exec.c — Privileged shell execution backend
 *
 * Probe order at bexec_init():
 *   1. rish  — ~/.shizuku/rish -c 'echo rish_ok'
 *              falls back to searching PATH for 'rish'
 *   2. ADB   — adb shell echo adb_ok
 *   3. Direct— plain popen; works for getprop, getenforce,
 *              world-readable /proc / /sys paths
 *
 * Thread-safety: bexec_init() is not thread-safe (call once from main
 * before spawning any threads).  bexec() and bexec_n() are read-only
 * after init and safe to call from any thread.
 */

#include "backend_exec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ── Internal state ───────────────────────────────────────────────────── */

static bexec_backend_t g_backend   = BEXEC_DIRECT;
static int             g_init_done = 0;
static char            g_rish_path[256] = "rish";   /* resolved at init */

/* ── Probe helpers ────────────────────────────────────────────────────── */

static int try_run(const char *cmd, const char *expect)
{
    FILE *f = popen(cmd, "r");
    if (!f) return 0;
    char buf[64] = {0};
    fgets(buf, sizeof(buf), f);
    pclose(f);
    return strstr(buf, expect) != NULL;
}

static int probe_rish(void)
{
    /* 1a: ~/.shizuku/rish */
    const char *home = getenv("HOME");
    if (home) {
        char path[256], probe[512];
        snprintf(path,  sizeof(path),  "%s/.shizuku/rish", home);
        snprintf(probe, sizeof(probe), "%s -c 'echo rish_ok' 2>/dev/null", path);
        if (access(path, X_OK) == 0 && try_run(probe, "rish_ok")) {
            snprintf(g_rish_path, sizeof(g_rish_path), "%s", path);
            return 1;
        }
    }

    /* 1b: rish anywhere in PATH */
    if (try_run("rish -c 'echo rish_ok' 2>/dev/null", "rish_ok")) {
        snprintf(g_rish_path, sizeof(g_rish_path), "rish");
        return 1;
    }

    return 0;
}

static int probe_adb(void)
{
    return try_run("adb shell echo adb_ok 2>/dev/null", "adb_ok");
}

/* ── Public lifecycle ─────────────────────────────────────────────────── */

void bexec_init(void)
{
    if (g_init_done) return;
    g_init_done = 1;

    if (probe_rish()) {
        g_backend = BEXEC_RISH;
    } else if (probe_adb()) {
        g_backend = BEXEC_ADB;
    } else {
        g_backend = BEXEC_DIRECT;
    }

    printf("[BEXEC] privileged shell backend: %s", bexec_backend_name());
    if (g_backend == BEXEC_RISH)
        printf("  (%s)", g_rish_path);
    if (g_backend == BEXEC_DIRECT)
        printf("  (no privilege — some commands may return empty output)");
    printf("\n");
    fflush(stdout);
}

bexec_backend_t bexec_backend(void)  { return g_backend; }

const char *bexec_backend_name(void)
{
    switch (g_backend) {
        case BEXEC_RISH:   return "rish";
        case BEXEC_ADB:    return "adb";
        case BEXEC_DIRECT: return "direct";
        default:           return "unknown";
    }
}

/* ── Single-quote escaping (for rish -c '…') ─────────────────────────── *
 *
 * Replace every ' with '\'' so the command can be safely wrapped in
 * single quotes on the rish / adb command line.
 *
 * Example:  dumpsys alarm | grep 'RTC'
 *        →  dumpsys alarm | grep '\''RTC'\''
 * ─────────────────────────────────────────────────────────────────────── */

static void sq_escape(const char *src, char *dst, size_t dstlen)
{
    size_t i = 0;
    while (*src && i + 5 < dstlen) {
        if (*src == '\'') {
            dst[i++] = '\'';
            dst[i++] = '\\';
            dst[i++] = '\'';
            dst[i++] = '\'';
        } else {
            dst[i++] = *src;
        }
        src++;
    }
    dst[i] = '\0';
}

/* ── bexec_n ──────────────────────────────────────────────────────────── */

char *bexec_n(const char *cmd, size_t maxout)
{
    if (!cmd || !*cmd) return NULL;
    if (!g_init_done) bexec_init();

    /* Build the full shell invocation */
    char *full = NULL;
    size_t fullsz;

    switch (g_backend) {

        case BEXEC_RISH: {
            /* rish -c '<escaped-cmd>' 2>/dev/null */
            char escaped[8192];
            sq_escape(cmd, escaped, sizeof(escaped));
            fullsz = strlen(g_rish_path) + strlen(escaped) + 32;
            full   = malloc(fullsz);
            if (!full) return NULL;
            snprintf(full, fullsz, "%s -c '%s' 2>/dev/null", g_rish_path, escaped);
            break;
        }

        case BEXEC_ADB: {
            /* adb shell "<escaped-cmd>" 2>/dev/null
             * Double-quote escaping is simpler for adb: escape " and \ */
            char escaped[8192];
            {
                const char *s = cmd;
                char *d = escaped;
                while (*s && (d - escaped) < (int)sizeof(escaped) - 3) {
                    if (*s == '"' || *s == '\\') *d++ = '\\';
                    *d++ = *s++;
                }
                *d = '\0';
            }
            fullsz = strlen(escaped) + 32;
            full   = malloc(fullsz);
            if (!full) return NULL;
            snprintf(full, fullsz, "adb shell \"%s\" 2>/dev/null", escaped);
            break;
        }

        case BEXEC_DIRECT:
        default: {
            fullsz = strlen(cmd) + 16;
            full   = malloc(fullsz);
            if (!full) return NULL;
            snprintf(full, fullsz, "%s 2>/dev/null", cmd);
            break;
        }
    }

    /* Execute */
    FILE *f = popen(full, "r");
    free(full);
    if (!f) return NULL;

    char *buf = malloc(maxout);
    if (!buf) { pclose(f); return NULL; }

    size_t n = fread(buf, 1, maxout - 1, f);
    pclose(f);

    if (n == 0) { free(buf); return NULL; }
    buf[n] = '\0';
    return buf;
}

char *bexec(const char *cmd)
{
    return bexec_n(cmd, 65536);
}

/* ── bexec_read_file ──────────────────────────────────────────────────── */

char *bexec_read_file(const char *path)
{
    if (!path) return NULL;

    /* Fast path: try direct fopen (world-readable sysfs/proc nodes) */
    FILE *f = fopen(path, "r");
    if (f) {
        char *buf = malloc(4096);
        if (!buf) { fclose(f); return NULL; }
        size_t n = fread(buf, 1, 4095, f);
        fclose(f);
        if (n > 0) {
            buf[n] = '\0';
            return buf;
        }
        free(buf);
    }

    /* Fallback: run cat through the privileged backend */
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "cat '%s'", path);
    return bexec_n(cmd, 4096);
}
