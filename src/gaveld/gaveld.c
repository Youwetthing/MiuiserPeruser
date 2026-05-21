/*
 * gaveld.c — Judicial Scoring Daemon
 * MiuiserPeruser v2
 *
 * One process. Eight threads. One database. One socket.
 *
 * Thread pipeline:
 *   ingest_thread  → reads pipes/ingest.pipe
 *   scorer_thread  → score + case assembly (inline here)
 *   verdict_thread → 5-state machine
 *   consent_thread → termux-notification gate
 *   enforce_thread → rish/adb execution (called inline by consent/verdict)
 *   audit_thread   → internal affairs, every 300s
 *   decay_thread   → score decay, every 120s
 *
 * Socket: pipes/gaveld.sock
 *   IN:  SOURCE|SIGNAL|WEIGHT|CONTEXT  → score pipeline
 *   IN:  QUERY|STATUS                  → dump threat_scores
 *   IN:  QUERY|DECAY                   → immediate decay tick
 *   IN:  APPROVE|case_id               → consent approval
 *   IN:  DENY|case_id                  → consent denial
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#include "config.h"
#include "log.h"
#include "db.h"
#include "ingest.h"
#include "scorer.h"
#include "cases.h"
#include "verdict.h"
#include "consent.h"
#include "enforce.h"
#include "audit.h"
#include "decay.h"

/* ── Globals ─────────────────────────────────────────────────────────────── */

static volatile int  g_running      = 1;
static pthread_t     g_scorer_thread;

/* ── Signal handling ─────────────────────────────────────────────────────── */

static void handle_signal(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        glog("INFO", "signal %d received — shutting down", sig);
        g_running = 0;
    }
}

/* ── PID file ────────────────────────────────────────────────────────────── */

static void write_pid(void) {
    FILE *fp = fopen(PID_FILE, "w");
    if (!fp) { glog("WARN", "cannot write PID file %s", PID_FILE); return; }
    fprintf(fp, "%d\n", (int)getpid());
    fclose(fp);
}

static void remove_pid(void) {
    unlink(PID_FILE);
}

/* ── Scorer thread — ingest → score → case ───────────────────────────────── */

static void *scorer_thread_fn(void *arg) {
    (void)arg;
    glog("INFO", "scorer_thread started");

    ingest_record_t rec;
    while (ingest_dequeue(&rec)) {
        if (rec.is_query) {
            /* QUERY records handled by socket server — skip here */
            glog("DEBUG", "scorer_thread: skipping QUERY cmd=%s", rec.query_cmd);
            continue;
        }

        score_result_t result;
        if (scorer_process(rec.source, rec.signal,
                           rec.weight, rec.ctx, &result) == 0) {
            cases_assemble(&result, rec.signal, rec.ctx);
        }
    }

    glog("INFO", "scorer_thread stopped");
    return NULL;
}

/* ── Socket server ───────────────────────────────────────────────────────── */

static void handle_status(int client_fd) {
    db_threat_t rows[512];
    int nrows = 0;
    db_threat_all(rows, 512, &nrows);

    char line[512];
    for (int i = 0; i < nrows; i++) {
        snprintf(line, sizeof(line), "%s|%.2f|%s|%d|%d\n",
                 rows[i].source, rows[i].score, rows[i].state,
                 rows[i].prior_jails, rows[i].prior_quarantines);
        write(client_fd, line, strlen(line));
    }
    write(client_fd, "END\n", 4);
}

static void handle_client(int client_fd) {
    char buf[BUF_SIZE];
    ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
    if (n <= 0) return;
    buf[n] = '\0';

    /* Strip trailing newline */
    char *nl = strchr(buf, '\n');
    if (nl) *nl = '\0';

    if (strncmp(buf, "QUERY|STATUS", 12) == 0) {
        handle_status(client_fd);

    } else if (strncmp(buf, "QUERY|DECAY", 11) == 0) {
        decay_tick();
        write(client_fd, "OK\n", 3);

    } else if (strncmp(buf, "APPROVE|", 8) == 0) {
        consent_reply(buf + 8, 1);
        write(client_fd, "OK\n", 3);

    } else if (strncmp(buf, "DENY|", 5) == 0) {
        consent_reply(buf + 5, 0);
        write(client_fd, "OK\n", 3);

    } else {
        /* Treat as a direct signal: SOURCE|SIGNAL|WEIGHT|CONTEXT */
        ingest_record_t rec;
        memset(&rec, 0, sizeof(rec));

        char tmp[BUF_SIZE];
        strncpy(tmp, buf, sizeof(tmp) - 1);
        char *src = strtok(tmp,  "|");
        char *sig = strtok(NULL, "|");
        char *wgt = strtok(NULL, "|");
        char *ctx = strtok(NULL, "\0");

        if (src && sig) {
            strncpy(rec.source, src, sizeof(rec.source) - 1);
            strncpy(rec.signal, sig, sizeof(rec.signal) - 1);
            rec.weight = wgt ? atof(wgt) : 0.0;
            if (ctx) strncpy(rec.ctx, ctx, sizeof(rec.ctx) - 1);

            score_result_t result;
            if (scorer_process(rec.source, rec.signal,
                               rec.weight, rec.ctx, &result) == 0) {
                char resp[256];
                scorer_format(&result, resp, sizeof(resp));
                write(client_fd, resp, strlen(resp));
                cases_assemble(&result, rec.signal, rec.ctx);
            } else {
                write(client_fd, "ERROR\n", 6);
            }
        } else {
            write(client_fd, "ERROR\n", 6);
        }
    }
}

static int setup_socket(void) {
    unlink(GAVELD_SOCK);

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        glog("ERROR", "socket() failed: %s", strerror(errno));
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, GAVELD_SOCK, sizeof(addr.sun_path) - 1);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        glog("ERROR", "bind(%s) failed: %s", GAVELD_SOCK, strerror(errno));
        close(fd);
        return -1;
    }

    chmod(GAVELD_SOCK, 0600);
    listen(fd, BACKLOG);
    glog("INFO", "socket listening: %s", GAVELD_SOCK);
    return fd;
}

/* ── Main ────────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    /* Signal handlers */
    signal(SIGTERM, handle_signal);
    signal(SIGINT,  handle_signal);
    signal(SIGPIPE, SIG_IGN);

    /* Bootstrap */
    if (log_open() != 0) { fprintf(stderr, "gaveld: log_open failed\n"); return 1; }
    glog("INFO", "gaveld %s starting — base=%s", GAVELD_VERSION, BASE_DIR);

    if (db_open()        != 0) { glog("ERROR", "db_open failed");        return 1; }
    if (db_init_schema() != 0) { glog("ERROR", "db_init_schema failed"); return 1; }

    enforce_init();
    write_pid();

    /* Start threads */
    if (decay_start()   != 0) { glog("ERROR", "decay_start failed");   return 1; }
    if (audit_start()   != 0) { glog("ERROR", "audit_start failed");   return 1; }
    if (ingest_start()  != 0) { glog("ERROR", "ingest_start failed");  return 1; }
    if (verdict_start() != 0) { glog("ERROR", "verdict_start failed"); return 1; }
    if (consent_start() != 0) { glog("ERROR", "consent_start failed"); return 1; }

    if (pthread_create(&g_scorer_thread, NULL, scorer_thread_fn, NULL) != 0) {
        glog("ERROR", "scorer_thread pthread_create failed");
        return 1;
    }

    /* Socket server */
    int srv_fd = setup_socket();
    if (srv_fd < 0) return 1;

    glog("INFO", "gaveld ready — all threads running");

    /* Main accept loop */
    while (g_running) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(srv_fd, &fds);

        struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
        int ready = select(srv_fd + 1, &fds, NULL, NULL, &tv);
        if (ready <= 0) continue;

        int client_fd = accept(srv_fd, NULL, NULL);
        if (client_fd < 0) continue;

        handle_client(client_fd);
        close(client_fd);
    }

    /* Graceful shutdown — reverse of start order */
    glog("INFO", "gaveld shutting down");
    close(srv_fd);
    unlink(GAVELD_SOCK);

    consent_stop();
    verdict_stop();
    ingest_stop();
    pthread_join(g_scorer_thread, NULL);
    audit_stop();
    decay_stop();

    db_checkpoint();
    db_close();
    remove_pid();

    glog("INFO", "gaveld stopped cleanly");
    log_close();
    return 0;
}
