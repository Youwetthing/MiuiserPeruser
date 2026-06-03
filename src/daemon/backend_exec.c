/*
 * backend_exec.c — Privileged command execution backend
 *
 * Probe order at bexec_init():
 *   1. rish  (Shizuku shell — full Android privileges)
 *   2. ADB   (adb -s 127.0.0.1:5555 shell — loopback, reliable)
 *   3. direct (no privilege — fallback only)
 *
 * Thread-safety: bexec_init() is not thread-safe (call once from main
 * before spawning threads).  bexec() / bexec_n() are re-entrant once
 * init is complete.
 */

#define _GNU_SOURCE
#include "backend_exec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* ── Internal state ───────────────────────────────────────────────────────── */

static BACKEND_TYPE    g_backend        = BACKEND_DIRECT;
static int             g_init_done      = 0;
static char            g_rish_path[256] = "";
static int             g_rish_disabled  = 0;

#define ADB_TRANSPORT  "adb -s 127.0.0.1:5555 shell"
#define RISH_TIMEOUT   2
#define TIMEOUT_BIN    "/data/data/com.termux/files/usr/bin/timeout"

/* ── Timeout availability ─────────────────────────────────────────────────── */

static int has_timeout_cmd(void)
{
    static int cached = -1;
    if (cached == -1)
        cached = (access(TIMEOUT_BIN, X_OK) == 0) ? 1 : 0;
    return cached;
}

/* ── try_run ──────────────────────────────────────────────────────────────── */

/*
 * Probe commands are internally controlled and safe (no single quotes).
 * Uses exact strcmp match to avoid false positives.
 */
static int try_run(const char *cmd, const char *expected_out)
{
    char wrapped[1024];

    if (has_timeout_cmd()) {
        snprintf(wrapped, sizeof(wrapped),
                 TIMEOUT_BIN " %d sh -c '%s' 2>/dev/null",
                 RISH_TIMEOUT, cmd);
    } else {
        snprintf(wrapped, sizeof(wrapped), "sh -c '%s' 2>/dev/null", cmd);
    }

    FILE *f = popen(wrapped, "r");
    if (!f) return 0;

    char buf[128] = {0};
    int matched = 0;
    if (fgets(buf, sizeof(buf), f)) {
        buf[strcspn(buf, "\r\n")] = '\0';
        matched = (strcmp(buf, expected_out) == 0);
    }
    pclose(f);
    return matched;
}

/* ── rish probe ───────────────────────────────────────────────────────────── */

static int probe_rish(void)
{
    if (g_rish_disabled) return 0;

    setenv("RISH_APPLICATION_ID", "com.termux", 1);

    const char *home = getenv("HOME");
    if (!home) { g_rish_disabled = 1; return 0; }

    const char *candidates[] = {
        "%s/Rish/rish",
        "%s/rish",
        NULL
    };

    for (int i = 0; candidates[i]; i++) {
        char path[256], dex[256];

        snprintf(path, sizeof(path), candidates[i], home);
        if (access(path, X_OK) != 0) continue;

        /* Safe DEX path construction */
        char *slash = strrchr(path, '/');
        if (!slash) continue;
        size_t dirlen = (size_t)(slash - path);
        snprintf(dex, sizeof(dex), "%.*s/rish_shizuku.dex", (int)dirlen, path);

        /* DEX must exist */
        struct stat st;
        if (stat(dex, &st) != 0) continue;

        /* Android 14+ rejects writable DEX — use chmod() not system() */
        if (st.st_mode & (S_IWUSR | S_IWGRP | S_IWOTH)) {
            if (chmod(dex, 0400) != 0) continue;
            /* Re-stat to confirm */
            if (stat(dex, &st) != 0) continue;
            if (st.st_mode & (S_IWUSR | S_IWGRP | S_IWOTH)) continue;
        }

        if (access(dex, R_OK) != 0) continue;

        /*
         * Probe command: avoid nesting single quotes inside try_run's
         * sh -c '...' wrapper. Build the probe so it only uses double
         * quotes internally — rish -c uses double quotes here.
         */
        char probe[512];
        snprintf(probe, sizeof(probe),
                 "RISH_APPLICATION_ID=com.termux %s -c \"echo rish_ok\"",
                 path);

        if (try_run(probe, "rish_ok")) {
            snprintf(g_rish_path, sizeof(g_rish_path), "%s", path);
            return 1;
        }

#ifdef BEXEC_DEBUG
        fprintf(stderr, "[BEXEC DEBUG] rish probe failed for: %s\n", path);
#endif
    }

    g_rish_disabled = 1;
    return 0;
}

/* ── ADB probe ────────────────────────────────────────────────────────────── */

static int probe_adb(void)
{
    return try_run(ADB_TRANSPORT " echo adb_ok", "adb_ok");
}

/* ── bexec_init ───────────────────────────────────────────────────────────── */

void bexec_init(void)
{
    if (g_init_done) return;
    g_init_done = 1;

    setenv("RISH_APPLICATION_ID", "com.termux", 1);

    if (probe_rish()) {
        g_backend = BACKEND_RISH;
        printf("[BEXEC] privileged shell backend: rish  (%s)\n", g_rish_path);
        return;
    }

    printf("[BEXEC] rish probe failed — trying ADB\n");

    if (probe_adb()) {
        g_backend = BACKEND_ADB;
        printf("[BEXEC] privileged shell backend: adb  (" ADB_TRANSPORT ")\n");
        return;
    }

    printf("[BEXEC] adb probe failed\n");
    printf("[BEXEC] privileged shell backend: direct"
           "  (no privilege — some commands may return empty output)\n");
    g_backend = BACKEND_DIRECT;
}

/* ── escape_for_adb ───────────────────────────────────────────────────────── */

/*
 * Escapes " and \ for use inside ADB double-quoted shell argument.
 * Note: $ and backticks are intentionally NOT escaped because daemon
 * commands (dumpsys, getprop, cat, ps) rely on shell expansion.
 * For v2: consider full escaping or passing via stdin.
 */
static char *escape_for_adb(const char *cmd)
{
    size_t len = strlen(cmd);
    char *out = malloc(len * 2 + 1);
    if (!out) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (cmd[i] == '"' || cmd[i] == '\\')
            out[j++] = '\\';
        out[j++] = cmd[i];
    }
    out[j] = '\0';
    return out;
}

/* ── escape_for_rish ──────────────────────────────────────────────────────── */

static char *escape_for_rish(const char *cmd)
{
    size_t len = strlen(cmd);
    char *out = malloc(len * 4 + 1);
    if (!out) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (cmd[i] == '\'') {
            out[j++] = '\'';
            out[j++] = '\\';
            out[j++] = '\'';
            out[j++] = '\'';
        } else {
            out[j++] = cmd[i];
        }
    }
    out[j] = '\0';
    return out;
}

/* ── bexec_n ──────────────────────────────────────────────────────────────── */

char *bexec_n(const char *cmd, size_t max_bytes)
{
    if (!g_init_done) bexec_init();

    size_t cap = (max_bytes > 0) ? max_bytes + 1 : 65536 + 1;
    char  *out = malloc(cap);
    if (!out) return NULL;
    out[0] = '\0';

    char  *full = NULL;
    size_t fullsz;

    if (g_backend == BACKEND_RISH) {
        char *escaped = escape_for_rish(cmd);
        if (!escaped) { free(out); return NULL; }
        fullsz = strlen(g_rish_path) + strlen(escaped) + 64;
        full   = malloc(fullsz);
        if (!full) { free(escaped); free(out); return NULL; }
        snprintf(full, fullsz,
                 "RISH_APPLICATION_ID=com.termux %s -c '%s' 2>/dev/null",
                 g_rish_path, escaped);
        free(escaped);

    } else if (g_backend == BACKEND_ADB) {
        char *escaped = escape_for_adb(cmd);
        if (!escaped) { free(out); return NULL; }
        fullsz = strlen(ADB_TRANSPORT) + strlen(escaped) + 32;
        full   = malloc(fullsz);
        if (!full) { free(escaped); free(out); return NULL; }
        snprintf(full, fullsz,
                 ADB_TRANSPORT " \"%s\" 2>/dev/null", escaped);
        free(escaped);

    } else {
        fullsz = strlen(cmd) + 16;
        full   = malloc(fullsz);
        if (!full) { free(out); return NULL; }
        snprintf(full, fullsz, "%s 2>/dev/null", cmd);
    }

    FILE *f = popen(full, "r");
    free(full);
    if (!f) { free(out); return NULL; }

    size_t total = 0;
    char   tmp[4096];
    while (total < cap - 1 && fgets(tmp, sizeof(tmp), f)) {
        size_t n = strlen(tmp);
        if (total + n >= cap - 1) n = cap - 1 - total;
        memcpy(out + total, tmp, n);
        total += n;
    }
    out[total] = '\0';
    pclose(f);
    return out;
}

/* ── bexec ────────────────────────────────────────────────────────────────── */

char *bexec(const char *cmd)
{
    return bexec_n(cmd, 65536);
}

/* ── backend_reprobe ──────────────────────────────────────────────────────── */

void backend_reprobe(void)
{
    g_init_done     = 0;
    g_rish_disabled = 0;
    g_rish_path[0]  = '\0';
    g_backend       = BACKEND_DIRECT;
    bexec_init();
}

/* ── Accessors ────────────────────────────────────────────────────────────── */

BACKEND_TYPE backend_get(void) { return g_backend; }

const char *backend_name(BACKEND_TYPE b)
{
    switch (b) {
        case BACKEND_RISH:   return "rish";
        case BACKEND_ADB:    return "adb";
        case BACKEND_DIRECT: return "direct";
        default:             return "unknown";
    }
}

BACKEND_TYPE splinter_get_backend(void) { return g_backend; }
