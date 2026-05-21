#define _GNU_SOURCE
#include "cases.h"
#include "config.h"
#include "db.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

/* ── Verdict queue ───────────────────────────────────────────────────────── */

static case_record_t   g_queue[VERDICT_QUEUE_SIZE];
static int             g_head    = 0;
static int             g_tail    = 0;
static int             g_count   = 0;
static int             g_running = 1;
static pthread_mutex_t g_mutex   = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_cond    = PTHREAD_COND_INITIALIZER;

static void queue_push(const case_record_t *rec) {
    pthread_mutex_lock(&g_mutex);
    if (g_count >= VERDICT_QUEUE_SIZE) {
        glog("WARN", "verdict queue full — dropping oldest case");
        g_head = (g_head + 1) % VERDICT_QUEUE_SIZE;
        g_count--;
    }
    g_queue[g_tail] = *rec;
    g_tail = (g_tail + 1) % VERDICT_QUEUE_SIZE;
    g_count++;
    pthread_cond_signal(&g_cond);
    pthread_mutex_unlock(&g_mutex);
}

int cases_dequeue(case_record_t *out) {
    pthread_mutex_lock(&g_mutex);
    while (g_count == 0 && g_running)
        pthread_cond_wait(&g_cond, &g_mutex);

    if (g_count == 0) {
        pthread_mutex_unlock(&g_mutex);
        return 0;
    }

    *out   = g_queue[g_head];
    g_head = (g_head + 1) % VERDICT_QUEUE_SIZE;
    g_count--;
    pthread_mutex_unlock(&g_mutex);
    return 1;
}

void cases_stop(void) {
    pthread_mutex_lock(&g_mutex);
    g_running = 0;
    pthread_cond_broadcast(&g_cond);
    pthread_mutex_unlock(&g_mutex);
}

int cases_queue_depth(void) {
    pthread_mutex_lock(&g_mutex);
    int d = g_count;
    pthread_mutex_unlock(&g_mutex);
    return d;
}

/* ── Case ID generation ──────────────────────────────────────────────────── */

static void make_case_id(char *buf, int len) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    /* millisecond precision — matches april_o_neil.sh case_id format */
    long ms = (long)ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
    snprintf(buf, len, "case_%ld", ms);
}

/* ── Cap enforcement — evict oldest cases over limit ─────────────────────── */

static void enforce_cap(void) {
    const char *count_sql =
        "SELECT COUNT(*) FROM cases;";
    const char *evict_sql =
        "DELETE FROM cases WHERE case_id IN "
        "(SELECT case_id FROM cases ORDER BY created ASC LIMIT ?);";

    /* TODO: expose db_case_count() in db.h if cap enforcement proves needed.
       At 500 cases and real device signal rates this rarely fires. */
    (void)count_sql;
    (void)evict_sql;
}

/* ── Assembly ────────────────────────────────────────────────────────────── */

int cases_assemble(const score_result_t *result,
                   const char *signal,
                   const char *ctx) {
    time_t now = time(NULL);

    /* Build case record */
    case_record_t crec;
    memset(&crec, 0, sizeof(crec));

    make_case_id(crec.case_id, sizeof(crec.case_id));
    strncpy(crec.source, result->source, sizeof(crec.source) - 1);
    strncpy(crec.signal, signal,         sizeof(crec.signal) - 1);
    strncpy(crec.state,  result->state,  sizeof(crec.state)  - 1);
    crec.score = result->new_score;
    crec.delta = result->delta;
    crec.epoch = (long)now;
    if (ctx) strncpy(crec.ctx, ctx, sizeof(crec.ctx) - 1);

    /* Persist to DB */
    db_case_t dbc;
    memset(&dbc, 0, sizeof(dbc));
    strncpy(dbc.case_id, crec.case_id, sizeof(dbc.case_id) - 1);
    strncpy(dbc.source,  crec.source,  sizeof(dbc.source)  - 1);
    strncpy(dbc.signal,  crec.signal,  sizeof(dbc.signal)  - 1);
    strncpy(dbc.context, crec.ctx,     sizeof(dbc.context) - 1);
    strncpy(dbc.status, "PENDING_JUDGEMENT", sizeof(dbc.status) - 1);
    dbc.score   = crec.score;
    dbc.created = now;

    if (db_case_insert(&dbc) != 0)
        glog("WARN", "cases: db_case_insert failed for %s", crec.case_id);

    glog("INFO", "case_assembled id=%s src=%s sig=%s score=%.2f state=%s",
         crec.case_id, crec.source, crec.signal, crec.score, crec.state);

    /* CLEAN / WATCHED — log only, do not route to verdict */
    if (strcmp(crec.state, "CLEAN")   == 0 ||
        strcmp(crec.state, "WATCHED") == 0) {
        glog("INFO", "case_dismissed id=%s state=%s — below enforcement threshold",
             crec.case_id, crec.state);
        db_case_update_status(crec.case_id, "DISMISSED");
        return 0;
    }

    /* Enforce cap before pushing */
    enforce_cap();

    /* Push to verdict queue */
    queue_push(&crec);
    glog("INFO", "case_routed id=%s state=%s score=%.2f queue_depth=%d",
         crec.case_id, crec.state, crec.score, cases_queue_depth());

    return 0;
}
