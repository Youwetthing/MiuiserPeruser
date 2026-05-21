#define _GNU_SOURCE
#include "ingest.h"
#include "config.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <sys/stat.h>

/* ── Ring buffer queue ───────────────────────────────────────────────────── */

static ingest_record_t  g_queue[INGEST_QUEUE_SIZE];
static int              g_head    = 0;   /* next read position  */
static int              g_tail    = 0;   /* next write position */
static int              g_count   = 0;
static pthread_mutex_t  g_mutex   = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t   g_cond    = PTHREAD_COND_INITIALIZER;
static volatile int     g_running = 0;
static pthread_t        g_thread;

/* ── Queue ops ───────────────────────────────────────────────────────────── */

static int queue_push(const ingest_record_t *rec) {
    pthread_mutex_lock(&g_mutex);
    if (g_count >= INGEST_QUEUE_SIZE) {
        /* Drop oldest to make room — log the drop */
        glog("WARN", "ingest queue full — dropping oldest record");
        g_head = (g_head + 1) % INGEST_QUEUE_SIZE;
        g_count--;
    }
    g_queue[g_tail] = *rec;
    g_tail = (g_tail + 1) % INGEST_QUEUE_SIZE;
    g_count++;
    pthread_cond_signal(&g_cond);
    pthread_mutex_unlock(&g_mutex);
    return 0;
}

int ingest_dequeue(ingest_record_t *out) {
    pthread_mutex_lock(&g_mutex);
    while (g_count == 0 && g_running)
        pthread_cond_wait(&g_cond, &g_mutex);

    if (g_count == 0) {
        pthread_mutex_unlock(&g_mutex);
        return 0;   /* stopped and empty */
    }

    *out   = g_queue[g_head];
    g_head = (g_head + 1) % INGEST_QUEUE_SIZE;
    g_count--;
    pthread_mutex_unlock(&g_mutex);
    return 1;
}

int ingest_queue_depth(void) {
    pthread_mutex_lock(&g_mutex);
    int d = g_count;
    pthread_mutex_unlock(&g_mutex);
    return d;
}

/* ── Line parser ─────────────────────────────────────────────────────────── */

static int parse_line(const char *line, ingest_record_t *out) {
    memset(out, 0, sizeof(*out));

    /* QUERY|CMD */
    if (strncmp(line, "QUERY|", 6) == 0) {
        out->is_query = 1;
        strncpy(out->query_cmd, line + 6, sizeof(out->query_cmd) - 1);
        /* Trim trailing newline */
        char *nl = strchr(out->query_cmd, '\n');
        if (nl) *nl = '\0';
        return 1;
    }

    /* SOURCE|SIGNAL|WEIGHT|CONTEXT */
    char tmp[BUF_SIZE];
    strncpy(tmp, line, sizeof(tmp) - 1);

    char *src = strtok(tmp,  "|");
    char *sig = strtok(NULL, "|");
    char *wgt = strtok(NULL, "|");
    char *ctx = strtok(NULL, "\n");   /* remainder, may contain pipes */

    if (!src || !*src || !sig || !*sig) return 0;

    strncpy(out->source, src, sizeof(out->source) - 1);
    strncpy(out->signal, sig, sizeof(out->signal) - 1);
    out->weight = wgt ? atof(wgt) : 0.0;
    if (ctx) strncpy(out->ctx, ctx, sizeof(out->ctx) - 1);

    return 1;
}

/* ── FIFO open — bidirectional trick prevents blocking ───────────────────── */

static int open_fifo(void) {
    /* Ensure FIFO exists */
    struct stat st;
    if (stat(INGEST_PIPE, &st) != 0 || !S_ISFIFO(st.st_mode)) {
        unlink(INGEST_PIPE);
        if (mkfifo(INGEST_PIPE, 0600) != 0) {
            glog("ERROR", "mkfifo(%s) failed: %s", INGEST_PIPE, strerror(errno));
            return -1;
        }
        glog("INFO", "created ingest FIFO: %s", INGEST_PIPE);
    }

    /* O_RDWR on a FIFO keeps it open even with no writers — no blocking */
    int fd = open(INGEST_PIPE, O_RDWR);
    if (fd < 0) {
        glog("ERROR", "open(%s) failed: %s", INGEST_PIPE, strerror(errno));
        return -1;
    }
    return fd;
}

/* ── Ingest thread ───────────────────────────────────────────────────────── */

static void *ingest_thread(void *arg) {
    (void)arg;
    glog("INFO", "ingest_thread started — pipe=%s", INGEST_PIPE);

    while (g_running) {
        int fd = open_fifo();
        if (fd < 0) {
            sleep(2);
            continue;
        }

        FILE *fp = fdopen(fd, "r");
        if (!fp) {
            glog("ERROR", "fdopen failed: %s", strerror(errno));
            close(fd);
            sleep(2);
            continue;
        }

        char line[BUF_SIZE];
        while (g_running && fgets(line, sizeof(line), fp)) {
            if (line[0] == '\n' || line[0] == '\0') continue;

            ingest_record_t rec;
            if (!parse_line(line, &rec)) {
                glog("WARN", "ingest: malformed line: %.80s", line);
                continue;
            }

            if (rec.is_query)
                glog("DEBUG", "ingest: QUERY cmd=%s", rec.query_cmd);
            else
                glog("DEBUG", "ingest: src=%s sig=%s wgt=%.0f",
                     rec.source, rec.signal, rec.weight);

            queue_push(&rec);
        }

        fclose(fp);

        if (g_running) {
            glog("INFO", "ingest: pipe EOF — reopening");
            sleep(1);
        }
    }

    glog("INFO", "ingest_thread stopped");
    return NULL;
}

/* ── Lifecycle ───────────────────────────────────────────────────────────── */

int ingest_start(void) {
    g_running = 1;
    if (pthread_create(&g_thread, NULL, ingest_thread, NULL) != 0) {
        glog("ERROR", "ingest_thread pthread_create failed");
        g_running = 0;
        return -1;
    }
    return 0;
}

void ingest_stop(void) {
    g_running = 0;
    /* Wake any blocked dequeue calls */
    pthread_mutex_lock(&g_mutex);
    pthread_cond_broadcast(&g_cond);
    pthread_mutex_unlock(&g_mutex);
    pthread_join(g_thread, NULL);
}
