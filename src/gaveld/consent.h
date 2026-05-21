#ifndef GAVELD_CONSENT_H
#define GAVELD_CONSENT_H

/*
 * consent.h — termux-notification gate
 *
 * Receives consent_request_t from verdict_thread via consent_enqueue().
 * Sends termux-notification, polls for APPROVE|case_id or DENY|case_id
 * arriving via the gaveld socket (dispatched by gaveld.c → consent_reply()).
 * On approval: calls verdict_emit() + enforce_execute().
 * On denial:   logs, dismisses.
 * On timeout:  holds and re-notifies every CONSENT_RENOTIFY_INTERVAL seconds.
 *
 * On startup: reloads any pending entries from consent_queue DB table
 * so outstanding requests survive a gaveld restart.
 */

#include <time.h>

typedef struct {
    char   case_id[32];
    char   source[256];
    char   verdict[32];
    double score;
    time_t queued;
    int    timeout_secs;      /* 0 = wait indefinitely */
    char   timeout_action[16];/* always "hold" — re-notify */
} consent_request_t;

int  consent_start(void);
void consent_stop(void);

/* Called by verdict_thread — enqueues a pending request */
void consent_enqueue(const consent_request_t *req);

/* Called by gaveld.c socket server on APPROVE|case_id or DENY|case_id */
void consent_reply(const char *case_id, int approved);

#endif /* GAVELD_CONSENT_H */
