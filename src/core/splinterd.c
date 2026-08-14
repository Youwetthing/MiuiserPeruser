/*
 * splinterd.c — Splinter sink daemon
 *
 * Listens on SPLINTER_SOCKET. Daemons connect and emit APRIL events.
 * Records inbound emissions to sewer.db. Does NOT re-dispatch to other
 * daemons — that was v3-era conductor behavior that no longer applies.
 *
 * Wire format (inbound only):
 *   APRIL|<source>|<type>|<payload>\n
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <poll.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <time.h>
#include <stdarg.h>
#include <stdbool.h>

#include "core_paths.h"
#include "sewer_db.h"

#define MP_PIPES_DIR      TURTLE_HOME "/pipes"
#define SPLINTER_SOCKET   MP_PIPES_DIR "/splinterd.sock"
#define MP_PIDS_DIR       TURTLE_HOME "/pipes/pids"
#define SEWER_DB_PATH     TURTLE_HOME "/Registry/sewer.db"

#define LISTEN_BACKLOG       8
#define RECV_BUF_SIZE        1284
#define SPLINTER_PREFIX      "APRIL"
#define SPLINTER_MAX_SOURCE  32
#define SPLINTER_MAX_TYPE    32
#define SPLINTER_MAX_PAYLOAD 1024

/* ── Event type ───────────────────────────────────────────────────────── */

typedef struct {
    char source[SPLINTER_MAX_SOURCE];
    char type[SPLINTER_MAX_TYPE];
    char payload[SPLINTER_MAX_PAYLOAD];
} splinter_event_t;

/* ── Globals ──────────────────────────────────────────────────────────── */

static int  g_debug   = 0;
volatile bool g_running = true;
static int  g_srv_fd  = -1;
static FILE *g_log_fp = NULL;

/* ── Logging ──────────────────────────────────────────────────────────── */

static void splinter_log(const char *level, const char *fmt, ...)
{
    char tsbuf[32];
    time_t t = time(NULL);
    strftime(tsbuf, sizeof(tsbuf), "%Y-%m-%dT%H:%M:%S", localtime(&t));

    va_list ap;
    va_start(ap, fmt);
    if (g_debug || strcmp(level, "ERROR") == 0) {
        fprintf(stderr, "[%s][SPLINTERD/%s] ", tsbuf, level);
        vfprintf(stderr, fmt, ap);
        fprintf(stderr, "\n");
    }
    if (g_log_fp) {
        fprintf(g_log_fp, "[%s][SPLINTERD/%s] ", tsbuf, level);
        vfprintf(g_log_fp, fmt, ap);
        fprintf(g_log_fp, "\n");
        fflush(g_log_fp);
    }
    va_end(ap);
}

/* ── Signals + PID ────────────────────────────────────────────────────── */

static void handle_sig(int sig)
{
    (void)sig;
    g_running = 0;
    if (g_srv_fd >= 0) shutdown(g_srv_fd, SHUT_RDWR);
}

/* Async-signal-safe fatal handler: write() only, no fprintf/dprintf/fsync/malloc. */
static void handle_fatal(int sig)
{
    char buf[64];
    int n = 0;
    const char *prefix = "SPLINTERD|FATAL|signal=";
    while (prefix[n]) { buf[n] = prefix[n]; n++; }
    if (sig >= 100) buf[n++] = '0' + (sig / 100);
    if (sig >= 10)  buf[n++] = '0' + ((sig / 10) % 10);
    buf[n++] = '0' + (sig % 10);
    buf[n++] = '\n';
    write(STDERR_FILENO, buf, (size_t)n);
    _exit(128 + sig);
}

static int g_lock_fd = -1;

static int acquire_singleton_lock(void)
{
    g_lock_fd = open(MP_PIDS_DIR "/splinterd.pid", O_CREAT | O_RDWR, 0644);
    if (g_lock_fd < 0) {
        splinter_log("ERROR", "cannot open pidfile for locking: %s",
                     MP_PIDS_DIR "/splinterd.pid");
        return -1;
    }
    if (flock(g_lock_fd, LOCK_EX | LOCK_NB) < 0) {
        splinter_log("ERROR", "another splinterd instance is already running (lock held on %s)",
                     MP_PIDS_DIR "/splinterd.pid");
        close(g_lock_fd);
        g_lock_fd = -1;
        return -1;
    }
    char buf[32];
    int n = snprintf(buf, sizeof(buf), "%d\n", (int)getpid());
    if (ftruncate(g_lock_fd, 0) < 0) { /* best-effort clear stale content */ }
    lseek(g_lock_fd, 0, SEEK_SET);
    write(g_lock_fd, buf, (size_t)n);
    return 0;
}

/* ── Protocol ─────────────────────────────────────────────────────────── */

static int parse_event(const char *line, splinter_event_t *out)
{
    char buf[RECV_BUF_SIZE];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    size_t len = strlen(buf);
    while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r'))
        buf[--len] = '\0';
    if (len == 0) return 0;

    char *saveptr;
    char *tok = strtok_r(buf, "|", &saveptr);
    if (!tok || strcmp(tok, SPLINTER_PREFIX) != 0) return 0;

    tok = strtok_r(NULL, "|", &saveptr);
    if (!tok || strlen(tok) == 0 || strlen(tok) >= SPLINTER_MAX_SOURCE) return 0;
    strncpy(out->source, tok, sizeof(out->source) - 1);

    tok = strtok_r(NULL, "|", &saveptr);
    if (!tok || strlen(tok) == 0 || strlen(tok) >= SPLINTER_MAX_TYPE) return 0;
    strncpy(out->type, tok, sizeof(out->type) - 1);

    tok = strtok_r(NULL, "", &saveptr);
    if (!tok) return 0;
    strncpy(out->payload, tok, sizeof(out->payload) - 1);

    return 1;
}

/* ── Record (was: Dispatch) ──────────────────────────────────────────── *
 * splinterd is a sink now, not a conductor. No forwarding to other
 * daemons — just record the emission to sewer.db. Payload is never
 * passed to sewer_db (data-minimization; see GDPR/sewer_db_policy.md).
 * ──────────────────────────────────────────────────────────────────── */

static void dispatch_event(const splinter_event_t *ev)
{
    sewer_db_record_emit(ev->source, ev->type, NULL);
}

/* ── Connection handler ───────────────────────────────────────────────── */

static void handle_connection(int client_fd)
{
    char buf[RECV_BUF_SIZE] = {0};
    size_t idx = 0;
    char c;
    ssize_t n;

    struct timeval tv = { .tv_sec = 2, .tv_usec = 0 };
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    while (idx < sizeof(buf) - 1) {
        n = recv(client_fd, &c, 1, 0);
        if (n <= 0) break;
        if (c == '\n') break;
        buf[idx++] = c;
    }
    buf[idx] = '\0';
    close(client_fd);

    if (idx == 0) return;

    splinter_event_t ev;
    memset(&ev, 0, sizeof(ev));
    if (!parse_event(buf, &ev)) {
        splinter_log("WARN", "dropped malformed event: %.64s", buf);
        return;
    }

    splinter_log("INFO", "src=%s type=%s", ev.source, ev.type);
    dispatch_event(&ev);
}

/* ── Socket setup ─────────────────────────────────────────────────────── */

static int create_server_socket(const char *path)
{
    unlink(path);
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        splinter_log("ERROR", "socket(): %s", strerror(errno));
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        splinter_log("ERROR", "bind(%s): %s", path, strerror(errno));
        close(fd);
        return -1;
    }

    chmod(path, 0600);

    if (listen(fd, LISTEN_BACKLOG) < 0) {
        splinter_log("ERROR", "listen(): %s", strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}

/* ── Main ─────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    const char *dbg = getenv("SPLINTER_DEBUG");
    if (dbg && strcmp(dbg, "1") == 0) g_debug = 1;

    const char *log_path = getenv("SPLINTER_LOG_PATH");
    if (log_path) g_log_fp = fopen(log_path, "a");

    signal(SIGINT,  handle_sig);
    signal(SIGTERM, handle_sig);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGSEGV, handle_fatal);
    signal(SIGBUS,  handle_fatal);
    signal(SIGILL,  handle_fatal);
    signal(SIGABRT, handle_fatal);
    signal(SIGFPE,  handle_fatal);

    if (acquire_singleton_lock() < 0) {
        return 1;
    }

    if (sewer_db_init(SEWER_DB_PATH) < 0) {
        splinter_log("ERROR", "failed to open sewer.db at %s — continuing, log-only mode",
                     SEWER_DB_PATH);
    }

    splinter_log("INFO", "starting on %s", SPLINTER_SOCKET);
    g_srv_fd = create_server_socket(SPLINTER_SOCKET);
    if (g_srv_fd < 0) return 1;

    splinter_log("INFO", "ready — pid=%d", (int)getpid());

    while (g_running) {
        int client = accept(g_srv_fd, NULL, NULL);
        if (client < 0) {
            if (errno == EINTR || errno == EBADF) break;
            continue;
        }
        handle_connection(client);
    }

    splinter_log("INFO", "shutting down");
    close(g_srv_fd);
    unlink(SPLINTER_SOCKET);
    sewer_db_close();
    if (g_lock_fd >= 0) {
        flock(g_lock_fd, LOCK_UN);
        close(g_lock_fd);
    }
    unlink(MP_PIDS_DIR "/splinterd.pid");

    if (g_log_fp) fclose(g_log_fp);
    return 0;
}
