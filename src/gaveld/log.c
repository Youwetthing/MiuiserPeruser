#define _GNU_SOURCE
#include "log.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>

static FILE           *g_fp    = NULL;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ── Internal: rotate if over size limit ─────────────────────────────────── */

static void maybe_rotate(void) {
    if (!g_fp) return;

    struct stat st;
    if (fstat(fileno(g_fp), &st) != 0) return;
    if (st.st_size < LOG_MAX_BYTES) return;

    /* Rotate: rename current → .1, open fresh */
    fclose(g_fp);
    g_fp = NULL;

    char rotated[512];
    snprintf(rotated, sizeof(rotated), "%s.1", LOG_PATH);
    rename(LOG_PATH, rotated);

    g_fp = fopen(LOG_PATH, "a");
    if (g_fp) setlinebuf(g_fp);
}

/* ── Open ────────────────────────────────────────────────────────────────── */

int log_open(void) {
    pthread_mutex_lock(&g_mutex);
    g_fp = fopen(LOG_PATH, "a");
    if (!g_fp) {
        pthread_mutex_unlock(&g_mutex);
        return -1;
    }
    setlinebuf(g_fp);
    pthread_mutex_unlock(&g_mutex);
    return 0;
}

void log_close(void) {
    pthread_mutex_lock(&g_mutex);
    if (g_fp) { fclose(g_fp); g_fp = NULL; }
    pthread_mutex_unlock(&g_mutex);
}

/* ── glog ────────────────────────────────────────────────────────────────── */

void glog(const char *level, const char *fmt, ...) {
    time_t now = time(NULL);
    char   msg[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    pthread_mutex_lock(&g_mutex);
    maybe_rotate();
    if (g_fp)
        fprintf(g_fp, "[GAVELD] %ld [%s] %s\n", (long)now, level, msg);
    pthread_mutex_unlock(&g_mutex);
}

void log_flush(void) {
    pthread_mutex_lock(&g_mutex);
    if (g_fp) fflush(g_fp);
    pthread_mutex_unlock(&g_mutex);
}
