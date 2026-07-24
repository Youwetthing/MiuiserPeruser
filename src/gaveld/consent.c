#define _GNU_SOURCE
#include "consent.h"
#include "config.h"
#include "verdict.h"
#include "enforce.h"
#include "db.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

/* ── Pending table ───────────────────────────────────────────────────────── */

#define MAX_PENDING CONSENT_QUEUE_SIZE

typedef struct {
    consent_request_t req;
    time_t            last_notified;
    int               active;
    int               reply;      /* 0=pending, 1=approved, -1=denied */
} pending_t;

static pending_t       g_pending[MAX_PENDING];
static pthread_mutex_t g_mutex   = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_cond    = PTHREAD_COND_INITIALIZER;
static pthread_t       g_thread;
static volatile int    g_running = 0;

/* ── Notification ────────────────────────────────────────────────────────── */

static void send_notification(const pending_t *p) {
    const consent_request_t *r = &p->req;

    /* Hash case_id to a stable notification ID (simple djb2) */
    unsigned long h = 5381;
    for (const char *c = r->case_id; *c; c++)
        h = ((h << 5) + h) + (unsigned char)*c;
    int notif_id = (int)(h & 0x7FFFFFFF);

    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "termux-notification"
        " --id %d"
        " --title \"gaveld — %s\""
        " --content \"%s | score: %.0f\""
        " --button1 \"Approve\""
        " --button1-action \"echo 'APPROVE|%s' | socat - UNIX-CONNECT:%s\""
        " --button2 \"Deny\""
        " --button2-action \"echo 'DENY|%s' | socat - UNIX-CONNECT:%s\""
        " 2>/dev/null",
        notif_id,
        r->verdict, r->source, r->score,
        r->case_id, GAVELD_SOCK,
        r->case_id, GAVELD_SOCK);

    int rc = system(cmd);
    /* rc != 0 means the user never saw the consent prompt, so the pending
     * case will time out with no decision rather than be approved/denied. */
    glog(rc == 0 ? "INFO" : "WARN",
         "NOTIF %s case=%s src=%s verdict=%s rc=%d",
         rc == 0 ? "sent" : "FAILED",
         r->case_id, r->source, r->verdict, rc);
}

static void dismiss_notification(const char *case_id) {
    unsigned long h = 5381;
    for (const char *c = case_id; *c; c++)
        h = ((h << 5) + h) + (unsigned char)*c;
    int notif_id = (int)(h & 0x7FFFFFFF);

    char cmd[256];
    snprintf(cmd, sizeof(cmd),
        "termux-notification-remove %d 2>/dev/null", notif_id);
    int rc = system(cmd);
    if (rc != 0)
        glog("WARN", "NOTIF dismiss failed case=%s rc=%d", case_id, rc);
}

/* ── Find slot by case_id (caller holds lock) ────────────────────────────── */

static pending_t *find_pending(const char *case_id) {
    for (int i = 0; i < MAX_PENDING; i++)
        if (g_pending[i].active &&
            strcmp(g_pending[i].req.case_id, case_id) == 0)
            return &g_pending[i];
    return NULL;
}

/* ── Public: enqueue ─────────────────────────────────────────────────────── */

void consent_enqueue(const consent_request_t *req) {
    pthread_mutex_lock(&g_mutex);

    /* Find free slot */
    pending_t *slot = NULL;
    for (int i = 0; i < MAX_PENDING; i++) {
        if (!g_pending[i].active) { slot = &g_pending[i]; break; }
    }

    if (!slot) {
        glog("WARN", "consent queue full — dropping case=%s", req->case_id);
        pthread_mutex_unlock(&g_mutex);
        return;
    }

    memset(slot, 0, sizeof(*slot));
    slot->req           = *req;
    slot->active        = 1;
    slot->reply         = 0;
    slot->last_notified = 0;   /* force immediate notification */

    pthread_cond_signal(&g_cond);
    pthread_mutex_unlock(&g_mutex);

    glog("INFO", "consent_enqueued case=%s src=%s verdict=%s",
         req->case_id, req->source, req->verdict);
}

/* ── Public: reply from socket server ────────────────────────────────────── */

void consent_reply(const char *case_id, int approved) {
    pthread_mutex_lock(&g_mutex);
    pending_t *p = find_pending(case_id);
    if (p) {
        p->reply = approved ? 1 : -1;
        pthread_cond_signal(&g_cond);
    } else {
        glog("WARN", "consent_reply: unknown case_id=%s", case_id);
    }
    pthread_mutex_unlock(&g_mutex);
}

/* ── Process one resolved entry (caller holds lock, releases before enforce) */

static void resolve_entry(pending_t *p) {
    consent_request_t req = p->req;
    int approved          = (p->reply == 1);

    p->active = 0;
    pthread_mutex_unlock(&g_mutex);

    dismiss_notification(req.case_id);
    db_consent_remove(req.case_id);

    if (approved) {
        glog("INFO", "CONSENT_APPROVED case=%s src=%s verdict=%s",
             req.case_id, req.source, req.verdict);
        verdict_emit(req.case_id, req.source, req.verdict,
                     req.score, 1, 1);
        enforce_execute(req.source, req.verdict,
                        req.case_id, req.score);
    } else {
        glog("INFO", "CONSENT_DENIED case=%s src=%s verdict=%s",
             req.case_id, req.source, req.verdict);
        verdict_emit(req.case_id, req.source, "CONSENT_DENIED",
                     req.score, 1, 0);
    }

    pthread_mutex_lock(&g_mutex);
}

/* ── Consent thread main loop ────────────────────────────────────────────── */

static void *consent_thread(void *arg) {
    (void)arg;
    glog("INFO", "consent_thread started");

    /* Reload pending from DB on startup */
    db_consent_t dbc[CONSENT_QUEUE_SIZE];
    int ndbc = 0;
    db_consent_pending(dbc, CONSENT_QUEUE_SIZE, &ndbc);
    for (int i = 0; i < ndbc; i++) {
        consent_request_t req;
        memset(&req, 0, sizeof(req));
        strncpy(req.case_id,        dbc[i].case_id,        sizeof(req.case_id)        - 1);
        strncpy(req.source,         dbc[i].source,         sizeof(req.source)         - 1);
        strncpy(req.verdict,        dbc[i].verdict,        sizeof(req.verdict)        - 1);
        strncpy(req.timeout_action, dbc[i].timeout_action, sizeof(req.timeout_action) - 1);
        req.score        = dbc[i].score;
        req.queued       = dbc[i].queued;
        req.timeout_secs = dbc[i].timeout_secs;
        consent_enqueue(&req);
    }
    if (ndbc > 0)
        glog("INFO", "consent_thread: reloaded %d pending from DB", ndbc);

    while (g_running) {
        pthread_mutex_lock(&g_mutex);

        /* Wait until there's something to do */
        while (g_running) {
            int has_work = 0;
            for (int i = 0; i < MAX_PENDING; i++)
                if (g_pending[i].active) { has_work = 1; break; }
            if (has_work) break;
            pthread_cond_wait(&g_cond, &g_mutex);
        }

        if (!g_running) {
            pthread_mutex_unlock(&g_mutex);
            break;
        }

        time_t now = time(NULL);

        for (int i = 0; i < MAX_PENDING; i++) {
            pending_t *p = &g_pending[i];
            if (!p->active) continue;

            /* Reply received */
            if (p->reply != 0) {
                resolve_entry(p);   /* releases + reacquires lock */
                continue;
            }

            /* Check hard limit for JAILED (24h) */
            if (p->req.timeout_secs == 0) {
                time_t age = now - p->req.queued;
                if (age > CONSENT_JAILED_HARD_LIMIT) {
                    glog("INFO",
                         "CONSENT_HARD_LIMIT case=%s — 24h elapsed, holding",
                         p->req.case_id);
                    p->req.queued = now; /* reset to re-notify */
                    p->last_notified = 0;
                }
            }

            /* Timeout-based hold (non-JAILED) */
            if (p->req.timeout_secs > 0) {
                time_t age = now - p->req.queued;
                if (age >= p->req.timeout_secs) {
                    glog("INFO",
                         "CONSENT_TIMEOUT case=%s action=hold — re-notifying",
                         p->req.case_id);
                    p->req.queued    = now;
                    p->last_notified = 0;
                }
            }

            /* Send / re-send notification */
            time_t since_notif = now - p->last_notified;
            if (p->last_notified == 0 ||
                since_notif >= CONSENT_RENOTIFY_INTERVAL) {
                send_notification(p);
                p->last_notified = now;
            }
        }

        pthread_mutex_unlock(&g_mutex);
        sleep(5);   /* poll every 5s */
    }

    glog("INFO", "consent_thread stopped");
    return NULL;
}

/* ── Lifecycle ───────────────────────────────────────────────────────────── */

int consent_start(void) {
    memset(g_pending, 0, sizeof(g_pending));
    g_running = 1;
    if (pthread_create(&g_thread, NULL, consent_thread, NULL) != 0) {
        glog("ERROR", "consent_thread pthread_create failed");
        g_running = 0;
        return -1;
    }
    return 0;
}

void consent_stop(void) {
    pthread_mutex_lock(&g_mutex);
    g_running = 0;
    pthread_cond_broadcast(&g_cond);
    pthread_mutex_unlock(&g_mutex);
    pthread_join(g_thread, NULL);
}
