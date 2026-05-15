/*
 * krangd.c — parse/format stage
 *
 * Topology:
 *   exec bridge  →  KRANG_SOCKET (listen)
 *                       ↓  truth_engine normalise/arbitrate
 *   DASHBOARD_SOCKET (connect out)
 *
 * Receives raw key=value payloads, normalises through truth_engine,
 * formats clean events and pushes to dashboard. Fire-and-forget
 * on the outbound leg — dashboard connection is not persistent.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>

#include "ipc_globals.h"
#include "../core/truth_engine.h"

static volatile int g_krang_running = 1;

static void handle_sig(int sig)
{
    (void)sig;
    g_krang_running = 0;
}

/* ── PID ──────────────────────────────────────────────────────────────── */

static void write_pid(void)
{
    FILE *f = fopen(KRANG_PID, "w");
    if (!f) return;
    fprintf(f, "%d\n", (int)getpid());
    fclose(f);
}

/* ── Framed I/O ───────────────────────────────────────────────────────── */

static ssize_t read_full(int fd, void *buf, size_t len)
{
    size_t off = 0;
    while (off < len) {
        ssize_t n = read(fd, (char *)buf + off, len - off);
        if (n <= 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        off += (size_t)n;
    }
    return (ssize_t)off;
}

static ssize_t write_full(int fd, const void *buf, size_t len)
{
    size_t off = 0;
    while (off < len) {
        ssize_t n = write(fd, (const char *)buf + off, len - off);
        if (n <= 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        off += (size_t)n;
    }
    return (ssize_t)off;
}

static char *recv_msg(int fd)
{
    uint32_t net_len;
    if (read_full(fd, &net_len, sizeof(net_len)) <= 0)
        return NULL;

    uint32_t len = ntohl(net_len);
    if (len == 0 || len > 1024 * 1024)
        return NULL;

    char *buf = malloc(len + 1);
    if (!buf)
        return NULL;

    if (read_full(fd, buf, len) <= 0) {
        free(buf);
        return NULL;
    }

    buf[len] = '\0';
    return buf;
}

static int send_msg(int fd, const char *msg)
{
    uint32_t len     = (uint32_t)strlen(msg);
    uint32_t net_len = htonl(len);
    if (write_full(fd, &net_len, sizeof(net_len)) <= 0) return -1;
    if (write_full(fd, msg, len)                  <= 0) return -1;
    return 0;
}

/* ── Formatting ───────────────────────────────────────────────────────── */

static void format_event(const truth_event_t *ev, char *out, size_t out_size)
{
    snprintf(out, out_size,
        "service=%s;source=%s;level=%d;voltage=%d;temp_c=%.1f;conflict=%d",
        ev->service,
        ev->source,
        ev->level,
        ev->voltage,
        ev->temp_c,
        ev->has_conflict);
}

/* ── Dashboard push (fire-and-forget) ────────────────────────────────── */

static void push_to_dashboard(const char *payload)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, DASHBOARD_SOCKET, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0)
        send_msg(fd, payload);
    else
        fprintf(stderr, "[KRANGD] dashboard not reachable — event dropped\n");

    close(fd);
}

/* ── Client handler ───────────────────────────────────────────────────── */

static void handle_client(int fd)
{
    char *raw = recv_msg(fd);
    if (!raw) return;

    truth_event_t ev;
    if (truth_engine_normalise(raw, &ev) != 0) {
        fprintf(stderr, "[KRANGD] normalise failed for payload: %.80s\n", raw);
        free(raw);
        return;
    }

    if (ev.has_conflict) {
        fprintf(stderr, "[KRANGD] conflict — service=%s source=%s "
                        "level=%d voltage=%d temp=%.1f\n",
                ev.service, ev.source,
                ev.level, ev.voltage, ev.temp_c);
    }

    char formatted[256];
    format_event(&ev, formatted, sizeof(formatted));
    push_to_dashboard(formatted);

    free(raw);
}

/* ── Main ─────────────────────────────────────────────────────────────── */

int main(void)
{
    signal(SIGTERM, handle_sig);
    signal(SIGINT,  handle_sig);
    signal(SIGPIPE, SIG_IGN);

    write_pid();

    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("krangd: socket"); return 1; }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, KRANG_SOCKET, sizeof(addr.sun_path) - 1);

    unlink(KRANG_SOCKET);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("krangd: bind");
        return 1;
    }

    if (listen(server_fd, 8) < 0) {
        perror("krangd: listen");
        return 1;
    }

    printf("krangd: ONLINE — %s\n", KRANG_SOCKET);

    while (g_krang_running) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            if (g_krang_running) perror("krangd: accept");
            break;
        }
        handle_client(client_fd);
        close(client_fd);
    }

    close(server_fd);
    unlink(KRANG_SOCKET);
    unlink(KRANG_PID);

    printf("krangd: offline\n");
    return 0;
}
