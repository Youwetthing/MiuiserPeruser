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

#include "local_exec.h"

/* ── Internal state ───────────────────────────────────────────────────────── */

static BACKEND_TYPE    g_backend        = BACKEND_DIRECT;
static int             g_init_done      = 0;
static char            g_rish_path[256] = "";
static int             g_rish_disabled  = 0;

#define ADB_PORT_DEFAULT   5555
#define ADB_PORT_SCAN_LO   30000
#define ADB_PORT_SCAN_HI   45000
#define PORTSCAN_PATH      "/data/data/com.termux/files/home/MiuiserPeruser/bin/portscan"
#define ADB_CLI_PATH       "/data/data/com.termux/files/home/.cargo/bin/adb_cli"

static char g_adb_transport[192]     = "";
static char g_adb_cli_transport[192] = "";
#define RISH_TIMEOUT   8   /* was 3 — measured live 2026-07-11: a healthy,
                              successful rish call took 2.6s just for a
                              trivial echo (Shizuku IPC round-trip cost on
                              this device). 3s left near-zero margin, so the
                              probe intermittently failed on perfectly good
                              rish sessions and silently fell back to
                              adb/direct. 8s gives real headroom without
                              hanging too long if rish is genuinely dead. */
#define TIMEOUT_BIN    "/data/data/com.termux/files/usr/bin/timeout"

#define BEXEC_CACHE_PATH "/data/data/com.termux/files/home/MiuiserPeruser/pipes/state/.bexec_backend"
#define BEXEC_CACHE_TTL  300  /* seconds — reprobe if stale */

/* ── resolve_adb_port ─────────────────────────────────────────────────────── *
 * Builds g_adb_transport/g_adb_cli_transport at runtime instead of a
 * hardcoded :5555. Uses local_exec() (not bexec()) since portscan needs
 * no privilege — routing it through bexec() would pay rish's measured
 * 2.6-3.1s Shizuku round-trip for a task that runs in milliseconds.
 * Falls back to ADB_PORT_DEFAULT if portscan isn't built yet or finds
 * nothing in range, so a missing binary degrades to prior behavior
 * instead of breaking bexec entirely.
 */
static void resolve_adb_port(void)
{
    int port = ADB_PORT_DEFAULT;

    if (access(PORTSCAN_PATH, X_OK) == 0) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "%s %d %d 127.0.0.1",
                 PORTSCAN_PATH, ADB_PORT_SCAN_LO, ADB_PORT_SCAN_HI);
        char *out = local_exec(cmd);
        if (out && out[0]) {
            int found = atoi(out);
            if (found > 0 && found <= 65535)
                port = found;
        }
        free(out);
    }

    snprintf(g_adb_transport, sizeof(g_adb_transport),
             "/data/data/com.termux/files/home/.cargo/bin/adb_cli tcp 127.0.0.1:%d shell",
             port);
    snprintf(g_adb_cli_transport, sizeof(g_adb_cli_transport),
             "timeout 5 %s tcp 127.0.0.1:%d shell", ADB_CLI_PATH, port);
}

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

            /* Drain whatever is already sitting in the pty's kernel buffer
             * before killing -- previously this branch killed and closed
             * immediately, discarding any output the child had already
             * produced but that hadn't been read yet (the poll loop below
             * only checks every PTY_POLL_MS, so up to that much
             * already-produced data could be sitting unread the instant
             * the deadline hits). Mirrors the drain the child_exited
             * branch already does. Doesn't rescue data the child hadn't
             * produced yet -- that's a real timeout, not a bug -- but
             * stops discarding data that was already there. */
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

static int probe_adb_cli(void)
{
    if (access(ADB_CLI_PATH, X_OK) != 0) return 0;
    char cmd[224];
    snprintf(cmd, sizeof(cmd), "%s echo adb_cli_ok", g_adb_cli_transport);
    return try_run(cmd, "adb_cli_ok");
}

/* ── ADB probe ────────────────────────────────────────────────────────────── */

static int probe_adb(void)
{
    char cmd[224];
    snprintf(cmd, sizeof(cmd), "%s echo adb_ok", g_adb_transport);
    return try_run(cmd, "adb_ok");
}

/* ── bexec cache ──────────────────────────────────────────────────────────── *
 * Without this, every daemon process re-runs the full probe chain from
 * scratch — each probe now costs real time even with the pty fix working
 * correctly (a healthy rish round-trip is still ~3s). Twelve daemons in a
 * scan round would mean twelve redundant probes for the same answer.
 * Format: "<unix_ts> <backend_int> <rish_path_or_dash>\n" — one line,
 * space-separated, readable by any daemon process without a JSON parser.
 * Best-effort: any read/write failure just falls through to a fresh probe.
 */

static int try_load_cache(void)
{
    FILE *f = fopen(BEXEC_CACHE_PATH, "r");
    if (!f) return 0;

    long ts = 0;
    int  backend = -1;
    char rish_path[256] = "-";

    int n = fscanf(f, "%ld %d %255s", &ts, &backend, rish_path);
    fclose(f);

    if (n != 3) return 0;
    if (backend < BACKEND_RISH || backend > BACKEND_ADB_CLI) return 0;
    if (time(NULL) - ts > BEXEC_CACHE_TTL) return 0;

    g_backend = (BACKEND_TYPE)backend;
    if (g_backend == BACKEND_RISH && strcmp(rish_path, "-") != 0)
        snprintf(g_rish_path, sizeof(g_rish_path), "%s", rish_path);

    return 1;
}

static void save_cache(void)
{
    FILE *f = fopen(BEXEC_CACHE_PATH, "w");
    if (!f) return; /* best-effort — pipes/state/ may not exist yet */
    fprintf(f, "%ld %d %s\n",
            (long)time(NULL),
            (int)g_backend,
            g_rish_path[0] ? g_rish_path : "-");
    fclose(f);
}

static void delete_cache(void)
{
    unlink(BEXEC_CACHE_PATH);
}

/* ── bexec_init ───────────────────────────────────────────────────────────── */

void bexec_init(void)
{
    if (g_init_done) return;
    g_init_done = 1;

    setenv("RISH_APPLICATION_ID", "com.termux", 1);

    if (try_load_cache()) {
        if (g_backend == BACKEND_ADB || g_backend == BACKEND_ADB_CLI)
            resolve_adb_port();
        printf("[BEXEC] privileged shell backend (cached): %s\n",
               backend_name(g_backend));
        return;
    }

    if (probe_rish()) {
        g_backend = BACKEND_RISH;
        printf("[BEXEC] privileged shell backend: rish  (%s)\n", g_rish_path);
        save_cache();
        return;
    }

    printf("[BEXEC] rish probe failed — trying adb_cli\n");
    resolve_adb_port();

    if (probe_adb_cli()) {
        g_backend = BACKEND_ADB_CLI;
        printf("[BEXEC] privileged shell backend: adb_cli  (%s)\n", g_adb_cli_transport);
        save_cache();
        return;
    }

    printf("[BEXEC] adb_cli probe failed — trying ADB\n");

    if (probe_adb()) {
        g_backend = BACKEND_ADB;
        printf("[BEXEC] privileged shell backend: adb  (%s)\n", g_adb_transport);
        save_cache();
        return;
    }

    printf("[BEXEC] adb probe failed\n");
    printf("[BEXEC] privileged shell backend: direct"
           "  (no privilege — some commands may return empty output)\n");
    g_backend = BACKEND_DIRECT;
    save_cache();
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

/* escape_for_rish() removed 2026-07-11 — confirmed zero call sites via
 * grep. Dead since the pty rewrite of bexec_n()'s RISH branch replaced
 * single-quote escaping with direct double-quote wrapping. Had a
 * malformed multi-character literal ('\'\0' instead of '\'') that the
 * compiler was correctly flagging as always-false — harmless only because
 * nothing called it. */

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
        fullsz = strlen(g_adb_cli_transport) + strlen(cmd) + 32;
        full   = malloc(fullsz);
        if (!full) { free(out); return NULL; }
        snprintf(full, fullsz,
                 "%s %s 2>/dev/null", g_adb_cli_transport, cmd);

    } else if (g_backend == BACKEND_ADB) {
        char *escaped = escape_for_adb(cmd);
        if (!escaped) { free(out); return NULL; }
        fullsz = strlen(g_adb_transport) + strlen(escaped) + 32;
        full   = malloc(fullsz);
        if (!full) { free(escaped); free(out); return NULL; }
        snprintf(full, fullsz,
                 "%s \"%s\" 2>/dev/null", g_adb_transport, escaped);
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

/* ── bexec_batch ─────────────────────────────────────────────────────────── */

int bexec_batch(bexec_batch_item_t *items, int n_items)
{
    bexec_init();

    if (!items || n_items <= 0 || n_items > BEXEC_MAX_BATCH) return -1;

    char combined[4096];
    combined[0] = '\0';
    size_t pos = 0;

    for (int i = 0; i < n_items; i++) {
        int n = snprintf(combined + pos, sizeof(combined) - pos,
                          "%secho __BX_%d__; %s",
                          i ? "; " : "", i, items[i].cmd);
        if (n < 0 || (size_t)n >= sizeof(combined) - pos) {
            fprintf(stderr, "[BEXEC_BATCH] combined command overflowed local 4096 buffer\n");
            return -1;
        }
        pos += (size_t)n;
    }

    /* Budget check applies unconditionally, not just when the RISH backend
     * is currently active. Only bexec_n()'s RISH branch has the fixed
     * 1600-byte rishcmd buffer (ADB_CLI/ADB build 'full' dynamically off
     * strlen(cmd)), but the active backend can flip between polls -- cache
     * TTL expiry, reprobe on failure -- so a batch that's safe on ADB_CLI
     * today must still be safe if RISH wins next poll. Enforcing here,
     * once, means callers never have to think about it. */
    if (strlen(combined) > BEXEC_CMD_BUDGET) {
        fprintf(stderr,
            "[BEXEC_BATCH] combined command %zu bytes exceeds %d-byte budget -- "
            "refusing (would silently truncate on RISH backend)\n",
            strlen(combined), BEXEC_CMD_BUDGET);
        return -1;
    }

    char *raw = bexec(combined);
    if (!raw) {
        for (int i = 0; i < n_items; i++) items[i].result = NULL;
        return -1;
    }

    char *cursor = raw;
    for (int i = 0; i < n_items; i++) {
        char marker[32];
        snprintf(marker, sizeof(marker), "__BX_%d__", i);

        char *start = strstr(cursor, marker);
        if (!start) { items[i].result = NULL; continue; }
        start += strlen(marker);
        while (*start == '\n' || *start == '\r') start++;

        char *end = NULL;
        if (i + 1 < n_items) {
            char next_marker[32];
            snprintf(next_marker, sizeof(next_marker), "__BX_%d__", i + 1);
            end = strstr(start, next_marker);
        }

        size_t len = end ? (size_t)(end - start) : strlen(start);
        while (len > 0 && (start[len-1] == '\n' || start[len-1] == '\r')) len--;

        char *res = malloc(len + 1);
        if (res) { memcpy(res, start, len); res[len] = '\0'; }
        items[i].result = res;

        cursor = end ? end : start + len;
    }

    free(raw);
    return 0;
}

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
    delete_cache();
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
