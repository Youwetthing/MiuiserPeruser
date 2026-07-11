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
#include <fcntl.h>
#include <sys/wait.h>
#include <poll.h>
#include <time.h>
#include <signal.h>
#include <errno.h>

/* ── Internal state ───────────────────────────────────────────────────────── */

static BACKEND_TYPE    g_backend        = BACKEND_DIRECT;
static int             g_init_done      = 0;
static char            g_rish_path[256] = "";
static int             g_rish_disabled  = 0;

#define ADB_TRANSPORT  "/data/data/com.termux/files/home/.cargo/bin/adb_cli tcp 127.0.0.1:5555 shell"
#define RISH_TIMEOUT   8   /* was 3 — measured live 2026-07-11: a healthy,
                              successful rish call took 2.6s just for a
                              trivial echo (Shizuku IPC round-trip cost on
                              this device). 3s left near-zero margin, so the
                              probe intermittently failed on perfectly good
                              rish sessions and silently fell back to
                              adb/direct. 8s gives real headroom without
                              hanging too long if rish is genuinely dead. */
#define TIMEOUT_BIN    "/data/data/com.termux/files/usr/bin/timeout"

/* ── Timeout availability ─────────────────────────────────────────────────── */

static int has_timeout_cmd(void)
{
    static int cached = -1;
    if (cached == -1)
        cached = (access(TIMEOUT_BIN, X_OK) == 0) ? 1 : 0;
    return cached;
}

/* ── run_via_pty ──────────────────────────────────────────────────────────── */

#define PTY_POLL_MS 100

static int run_via_pty(const char *cmd, char *out, size_t outsize, int timeout_sec)
{
    int master = posix_openpt(O_RDWR | O_NOCTTY);
    if (master < 0) return -1;

    if (grantpt(master) != 0 || unlockpt(master) != 0) {
        close(master);
        return -1;
    }

    char *slave_name = ptsname(master);
    if (!slave_name) {
        close(master);
        return -1;
    }

    char slave_path[256];
    snprintf(slave_path, sizeof(slave_path), "%s", slave_name);

    pid_t pid = fork();
    if (pid < 0) {
        close(master);
        return -1;
    }

    if (pid == 0) {
        setsid();

        int slave = open(slave_path, O_RDWR);
        if (slave < 0)
            _exit(127);

        dup2(slave, STDIN_FILENO);
        dup2(slave, STDOUT_FILENO);
        dup2(slave, STDERR_FILENO);

        if (slave > STDERR_FILENO)
            close(slave);

        close(master);

        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127);
    }

    size_t total = 0;
    out[0] = '\0';

    time_t deadline = time(NULL) + timeout_sec;

    int child_exited = 0;
    int child_status = 0;

    for (;;) {

        int remaining_ms = (int)((deadline - time(NULL)) * 1000);

        if (remaining_ms <= 0) {

            kill(-pid, SIGKILL);

            while (waitpid(pid, NULL, WNOHANG) == 0)
                usleep(10000);

            close(master);
            out[total] = '\0';
            return (int)total;
        }

        if (!child_exited) {

            pid_t w = waitpid(pid, &child_status, WNOHANG);

            if (w == pid)
                child_exited = 1;
            else if (w < 0 && errno != EINTR)
                child_exited = 1;
        }

        if (child_exited) {

            int flags = fcntl(master, F_GETFL, 0);
            fcntl(master, F_SETFL, flags | O_NONBLOCK);

            for (;;) {

                char tmp[4096];

                ssize_t n = read(master, tmp, sizeof(tmp));

                if (n <= 0)
                    break;

                if (total + (size_t)n < outsize - 1) {
                    memcpy(out + total, tmp, (size_t)n);
                    total += (size_t)n;
                }
            }

            close(master);
            out[total] = '\0';
            return (int)total;
        }

        struct pollfd pfd = {
            .fd = master,
            .events = POLLIN
        };

        int pr = poll(&pfd, 1,
                      remaining_ms > PTY_POLL_MS ?
                      PTY_POLL_MS :
                      remaining_ms);

        if (pr < 0) {
            if (errno == EINTR)
                continue;
            break;
        }

        if (pr == 0)
            continue;

        if (pfd.revents & (POLLIN | POLLHUP | POLLERR)) {

            char tmp[4096];

            ssize_t n = read(master, tmp, sizeof(tmp));

            if (n < 0) {

                if (errno == EAGAIN || errno == EINTR)
                    continue;

                break;
            }

            if (n == 0)
                continue;

            if (total + (size_t)n < outsize - 1) {
                memcpy(out + total, tmp, (size_t)n);
                total += (size_t)n;
            }
        }
    }

    kill(-pid, SIGKILL);

    while (waitpid(pid, NULL, WNOHANG) == 0)
        usleep(10000);

    close(master);

    out[total] = '\0';

    return (int)total;
}

/* ── try_run ──────────────────────────────────────────────────────────────── */

/*
 * Probe commands are internally controlled and safe (no single quotes).
 * Uses exact strcmp match to avoid false positives.
 */
static int try_run(const char *cmd, const char *expected_out)
{
    char buf[128] = {0};
    int n = run_via_pty(cmd, buf, sizeof(buf), RISH_TIMEOUT);
    if (n <= 0) return 0;
    buf[strcspn(buf, "\r\n")] = '\0';
    return (strcmp(buf, expected_out) == 0);
}

/* ── rish probe ───────────────────────────────────────────────────────────── */

static int probe_rish(void)
{
    if (g_rish_disabled) return 0;
    if (getenv("BEXEC_NO_RISH")) { g_rish_disabled = 1; return 0; }
    if (getenv("BEXEC_NO_RISH")) { g_rish_disabled = 1; return 0; }

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

        /* Shizuku IPC on this device is intermittently slow — confirmed
         * live 2026-07-11: identical probe commands succeeded in 2.6-3.1s
         * on some attempts and failed outright on others, even with an
         * 8s timeout. A single-shot probe treats normal jitter as "rish is
         * dead" and silently falls back to adb/direct. Retry a few times
         * before giving up. */
        int rish_ok = 0;
        for (int attempt = 1; attempt <= 2 && !rish_ok; attempt++) {
            if (try_run(probe, "rish_ok")) {
                rish_ok = 1;
                break;
            }
            if (attempt < 2) usleep(1500000); /* 1.5s — give Shizuku's IPC
                                                  room to release the prior
                                                  connection before retry */
        }
        if (rish_ok) {
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

/* ── adb_cli probe ────────────────────────────────────────────────────────── */

#define ADB_CLI_PATH "/data/data/com.termux/files/home/.cargo/bin/adb_cli"
#define ADB_CLI_TRANSPORT "timeout 5 " ADB_CLI_PATH " tcp 127.0.0.1:5555 shell"

static int probe_adb_cli(void)
{
    if (access(ADB_CLI_PATH, X_OK) != 0) return 0;
    return try_run(ADB_CLI_TRANSPORT " echo adb_cli_ok", "adb_cli_ok");
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

    printf("[BEXEC] rish probe failed — trying adb_cli\n");

    if (probe_adb_cli()) {
        g_backend = BACKEND_ADB_CLI;
        printf("[BEXEC] privileged shell backend: adb_cli  (" ADB_CLI_TRANSPORT ")\n");
        return;
    }

    printf("[BEXEC] adb_cli probe failed — trying ADB\n");

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
        if (cmd[i] == '\'\0') {
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
        char rishcmd[1600];
        snprintf(rishcmd, sizeof(rishcmd),
                 "RISH_APPLICATION_ID=com.termux %s -c \"%s\"",
                 g_rish_path, cmd);
        int n = run_via_pty(rishcmd, out, cap, RISH_TIMEOUT);
        if (n < 0) { out[0] = '\0'; }
        return out;

    } else if (g_backend == BACKEND_ADB_CLI) {
        fullsz = strlen(ADB_CLI_TRANSPORT) + strlen(cmd) + 32;
        full   = malloc(fullsz);
        if (!full) { free(out); return NULL; }
        snprintf(full, fullsz,
                 ADB_CLI_TRANSPORT " %s 2>/dev/null", cmd);

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
        case BACKEND_RISH:    return "rish";
        case BACKEND_ADB:     return "adb";
        case BACKEND_ADB_CLI: return "adb_cli";
        case BACKEND_DIRECT: return "direct";
        default:             return "unknown";
    }
}

BACKEND_TYPE splinter_get_backend(void) { return g_backend; }
