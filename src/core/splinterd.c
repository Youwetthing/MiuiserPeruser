/*
 * splinterd.c — Splinter conductor daemon
 *
 * Listens on SPLINTER_SOCKET. Daemons connect and emit APRIL events.
 * Dispatches to subscribers based on event type interest lists.
 *
 * Wire format (inbound + outbound):
 *   APRIL|<source>|<type>|<payload>\n
 *
 * Subscriber sockets defined below. Add new daemons to g_subscribers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <time.h>
#include <stdarg.h>
#include <stdbool.h>

#include "core_paths.h"

#define MP_PIPES_DIR      TURTLE_HOME "/pipes"
#define SPLINTER_SOCKET   MP_PIPES_DIR "/splinterd.sock"
#define MP_PIDS_DIR       TURTLE_HOME "/pipes/pids"

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

/* ── Subscriber table ─────────────────────────────────────────────────── *
 *
 * interests: list of event types this subscriber wants, or "*" for all.
 * sock_path: the UNIX socket splinterd connects to when forwarding.
 *            Daemons own their own listen sockets; splinterd is the client.
 *
 * Add new daemons here. Keep sock_path in sync with ipc_globals.h.
 * ──────────────────────────────────────────────────────────────────────── */

typedef struct {
    const char *name;
    const char *sock_path;
    const char *interests[8];
    int         enabled;
} splinter_sub_t;

static splinter_sub_t g_subscribers[] = {
    {
        .name      = "krangd",
        .sock_path = KRANG_SOCKET,
        .interests = { "sysstate", "anomaly", "connectivity_anomaly", NULL },
        .enabled   = 1,
    },
    {
        .name      = "rahzerd",
        .sock_path = MP_PIPES_DIR "/rahzerd.sock",
        .interests = { "wakelock", "alarm", NULL },
        .enabled   = 1,
    },
    {
        .name      = "footclan",
        .sock_path = MP_PIPES_DIR "/footclan.sock",
        .interests = { "*", NULL },
        .enabled   = 1,
    },
    { NULL, NULL, { NULL }, 0 }
};
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

static void write_pid(void)
{
    FILE *f = fopen(MP_PIDS_DIR "/splinterd.pid", "w");
    if (!f) return;
    fprintf(f, "%d\n", (int)getpid());
    fclose(f);
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

/* ── Dispatch ─────────────────────────────────────────────────────────── */

static void forward_to_sub(const splinter_sub_t *sub, const splinter_event_t *ev)
{
    if (!sub->enabled) return;

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return;
    fcntl(fd, F_SETFL, O_NONBLOCK);

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sub->sock_path, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        if (errno != EINPROGRESS && errno != ENOENT)
            splinter_log("WARN", "connect failed sub=%s: %s",
                         sub->name, strerror(errno));
        close(fd);
        return;
    }

    char wire[RECV_BUF_SIZE];
    int wlen = snprintf(wire, sizeof(wire), "%s|%s|%s|%s\n",
                        SPLINTER_PREFIX, ev->source, ev->type, ev->payload);
    if (write(fd, wire, (size_t)wlen) < 0)
        splinter_log("WARN", "write failed sub=%s", sub->name);

    close(fd);
}

static void dispatch_event(const splinter_event_t *ev)
{
    int routed = 0;
    for (int i = 0; g_subscribers[i].name != NULL; i++) {
        splinter_sub_t *sub = &g_subscribers[i];
        if (!sub->enabled) continue;
        for (int j = 0; j < 8 && sub->interests[j] != NULL; j++) {
            if (strcmp(sub->interests[j], "*") == 0 ||
                strcmp(sub->interests[j], ev->type) == 0) {
                forward_to_sub(sub, ev);
                routed++;
                break;
            }
        }
    }
    if (routed == 0)
        splinter_log("INFO", "unrouted event src=%s type=%s",
                     ev->source, ev->type);
}

/* ── Connection handler ───────────────────────────────────────────────── */

static void handle_connection(int client_fd)
{
    char buf[RECV_BUF_SIZE] = {0};
    size_t idx = 0;
    char c;
    ssize_t n;

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

    write_pid();

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
    unlink(MP_PIDS_DIR "/splinterd.pid");

    if (g_log_fp) fclose(g_log_fp);
    return 0;
}
