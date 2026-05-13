/*
 * backend_shizuku_direct.c
 *
 * Third privileged-shell route. Talks to a long-lived JVM running
 * ShizukuDirectLoader (see java-helper/src/main/java/ShizukuDirectLoader.java
 * and Build_Dex.sh in the repo root) over an AF_UNIX SOCK_STREAM at
 * SHZ_DIRECT_SOCKET_PATH.
 *
 * Wire protocol per command (matches the Java side):
 *   client -> server:  "<cmd>\n"
 *   server -> client:  "<bytes>\n<payload>"   (length-prefixed; <bytes> is
 *                                              decimal ASCII, <payload> is raw)
 *
 * The backend will:
 *   - On init: ensure the helper JVM is running (spawning app_process if not),
 *     wait for the socket to appear, and connect.
 *   - On send: write the command, read the framed response, print to stdout,
 *     return the helper's reported rc-tail if present, else 0.
 *   - On any IO error: close & lazily reconnect on the next send_cmd.
 *
 * Build is conditional: defining MIUISER_NO_SHIZUKU_DIRECT skips it (e.g. for
 * hosts that don't ship the helper dex).
 */

#include "../../include/backends/backend_common.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifndef SHZ_DIRECT_SOCKET_PATH
#define SHZ_DIRECT_SOCKET_PATH \
    "/data/data/com.termux/files/usr/tmp/miuiser-shizuku.sock"
#endif

#ifndef SHZ_DIRECT_DEX_PATH
#define SHZ_DIRECT_DEX_PATH \
    "/data/data/com.termux/files/home/MiuiserPeruser/rish_shizuku_direct.dex"
#endif

#ifndef SHZ_DIRECT_HOME
#define SHZ_DIRECT_HOME \
    "/data/data/com.termux/files/home"
#endif

#ifndef SHZ_DIRECT_LAUNCH_TIMEOUT_S
#define SHZ_DIRECT_LAUNCH_TIMEOUT_S 12
#endif

static int g_sock = -1;
static pid_t g_helper_pid = -1;

/* ------------------------------------------------------------------------- */
/* helpers                                                                   */
/* ------------------------------------------------------------------------- */

static int file_exists(const char *p) {
    struct stat st;
    return p && stat(p, &st) == 0;
}

static void shz_log(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[SHZ-DIRECT] ");
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

static int connect_unix(const char *path) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

/* spawn the helper JVM via app_process; returns pid (>0) or -1 */
static pid_t spawn_helper(void) {
    if (!file_exists(SHZ_DIRECT_DEX_PATH)) {
        shz_log("dex missing at %s", SHZ_DIRECT_DEX_PATH);
        return -1;
    }
    /* Build the classpath define ahead of time; app_process needs it. */
    char dprop[512];
    snprintf(dprop, sizeof(dprop), "-Djava.class.path=%s", SHZ_DIRECT_DEX_PATH);

    pid_t pid = fork();
    if (pid < 0) {
        shz_log("fork: %s", strerror(errno));
        return -1;
    }
    if (pid == 0) {
        /* child: detach stdio so a daemon parent isn't tied to the JVM's */
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            /* Keep stderr -- the helper logs there and we want it for triage. */
            close(devnull);
        }
        setsid();

        /* Inherit RISH_APPLICATION_ID from parent env if set; otherwise default. */
        if (!getenv("RISH_APPLICATION_ID")) {
            setenv("RISH_APPLICATION_ID", "com.termux", 1);
        }

        execl("/system/bin/app_process",
              "app_process",
              dprop,
              SHZ_DIRECT_HOME,
              "--nice-name=miuiser_shz_helper",
              "ShizukuDirectLoader",
              "--uds", SHZ_DIRECT_SOCKET_PATH,
              (char *)NULL);

        /* if we got here, exec failed */
        fprintf(stderr, "[SHZ-DIRECT][child] execl app_process: %s\n", strerror(errno));
        _exit(127);
    }
    return pid;
}

static int wait_for_socket(const char *path, int timeout_s) {
    int waited = 0;
    while (waited < timeout_s * 10) {
        if (file_exists(path)) {
            int fd = connect_unix(path);
            if (fd >= 0) return fd;
        }
        struct timespec ts = {0, 100 * 1000 * 1000}; /* 100ms */
        nanosleep(&ts, NULL);
        waited++;
    }
    return -1;
}

/* ------------------------------------------------------------------------- */
/* vtable                                                                    */
/* ------------------------------------------------------------------------- */

int shizuku_direct_init(void) {
    shz_log("init: looking for socket %s", SHZ_DIRECT_SOCKET_PATH);

    /* Try an existing helper first; cheap reconnect path. */
    if (file_exists(SHZ_DIRECT_SOCKET_PATH)) {
        g_sock = connect_unix(SHZ_DIRECT_SOCKET_PATH);
        if (g_sock >= 0) {
            shz_log("attached to existing helper");
            return 0;
        }
        /* socket file exists but is stale -- nuke it and respawn */
        unlink(SHZ_DIRECT_SOCKET_PATH);
    }

    /* Need to start the helper ourselves. */
    if (!file_exists(SHZ_DIRECT_DEX_PATH)) {
        shz_log("missing dex: %s", SHZ_DIRECT_DEX_PATH);
        return -1;
    }

    g_helper_pid = spawn_helper();
    if (g_helper_pid <= 0) {
        shz_log("could not spawn helper");
        return -1;
    }
    shz_log("spawned helper pid=%d, waiting for socket...", (int)g_helper_pid);

    g_sock = wait_for_socket(SHZ_DIRECT_SOCKET_PATH, SHZ_DIRECT_LAUNCH_TIMEOUT_S);
    if (g_sock < 0) {
        shz_log("helper never opened socket; giving up");
        return -1;
    }
    shz_log("ready (sock=%d, pid=%d)", g_sock, (int)g_helper_pid);
    return 0;
}

/* read exactly n bytes; returns 0 on success, -1 on error/EOF */
static int read_exact(int fd, char *buf, size_t n) {
    size_t off = 0;
    while (off < n) {
        ssize_t r = read(fd, buf + off, n - off);
        if (r <= 0) return -1;
        off += (size_t)r;
    }
    return 0;
}

/* read the length-prefixed response into a heap buffer (caller frees) */
static char *read_framed(int fd, size_t *out_len) {
    char header[32];
    size_t hpos = 0;
    while (hpos < sizeof(header) - 1) {
        char c;
        ssize_t r = read(fd, &c, 1);
        if (r <= 0) return NULL;
        if (c == '\n') break;
        header[hpos++] = c;
    }
    header[hpos] = '\0';
    long n = strtol(header, NULL, 10);
    if (n < 0 || n > (long)(64 * 1024 * 1024)) return NULL; /* sanity cap 64M */
    char *buf = malloc((size_t)n + 1);
    if (!buf) return NULL;
    if (n > 0 && read_exact(fd, buf, (size_t)n) != 0) {
        free(buf);
        return NULL;
    }
    buf[n] = '\0';
    if (out_len) *out_len = (size_t)n;
    return buf;
}

int shizuku_direct_send(const char *cmd) {
    if (!cmd) return -1;

    /* Lazy reconnect if the prior connection died. */
    if (g_sock < 0) {
        if (shizuku_direct_init() != 0) return -1;
    }

    /* Write "<cmd>\n" */
    size_t L = strlen(cmd);
    char *line = malloc(L + 2);
    if (!line) return -1;
    memcpy(line, cmd, L);
    line[L] = '\n';
    line[L + 1] = '\0';

    ssize_t w = write(g_sock, line, L + 1);
    free(line);
    if (w < 0 || (size_t)w != L + 1) {
        shz_log("write failed: %s", strerror(errno));
        close(g_sock); g_sock = -1;
        return -1;
    }

    size_t resp_len = 0;
    char *resp = read_framed(g_sock, &resp_len);
    if (!resp) {
        shz_log("framed read failed");
        close(g_sock); g_sock = -1;
        return -1;
    }
    /* Pass through to the caller's stdout (the daemon's logging layer). */
    if (resp_len) fwrite(resp, 1, resp_len, stdout);
    fputc('\n', stdout);
    fflush(stdout);

    /* The helper appends "[rc=N]" if the child exited non-zero. */
    int rc = 0;
    char *tag = strstr(resp, "[rc=");
    if (tag) rc = (int)strtol(tag + 4, NULL, 10);
    free(resp);
    return rc;
}

/* Exposed vtable -- selector picks this up when our dex+socket are present. */
const backend_vtable_t backend_shizuku_direct_vtable = {
    .name     = "SHIZUKU_DIRECT",
    .init     = shizuku_direct_init,
    .send_cmd = shizuku_direct_send,
};
