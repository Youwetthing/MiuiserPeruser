/*
 * backend_exec.h — Privileged shell execution backend
 *
 * Probe order at bexec_init():
 *   1. rish  (~/Rish/rish — Shizuku shell, Android 14+ compatible)
 *   2. ADB   (adb -s 127.0.0.1:5555 shell — loopback transport)
 *   3. direct (no privilege — fallback only)
 */

#ifndef DAEMON_BACKEND_EXEC_H
#define DAEMON_BACKEND_EXEC_H

#include <stddef.h>

typedef enum {
    BACKEND_RISH    = 0,
    BACKEND_ADB     = 1,
    BACKEND_DIRECT  = 2,
    BACKEND_ADB_CLI = 3,
} BACKEND_TYPE;

void          bexec_init(void);
void          backend_reprobe(void);
BACKEND_TYPE  backend_get(void);
const char   *backend_name(BACKEND_TYPE b);
BACKEND_TYPE  splinter_get_backend(void);

char *bexec(const char *cmd);
char *bexec_n(const char *cmd, size_t maxout);
char *bexec_read_file(const char *path);


/* ── bexec_batch: multi-command batching within RISH's byte budget ────────
 * See backend_exec.c for rationale. Do not hand-combine commands with
 * bexec_n() directly -- this exists specifically to enforce
 * BEXEC_CMD_BUDGET and avoid the silent-truncation failure mode found
 * 2026-07 (tigerclawd/shredderd batching incident: rishcmd[1600] silently
 * truncated combined commands with no error, producing false clean data).
 */
#define BEXEC_CMD_BUDGET 1450
#define BEXEC_MAX_BATCH  12

typedef struct {
    const char *label;   /* for debug/logging only */
    const char *cmd;     /* input: command to run */
    char       *result;  /* output: malloc'd, NULL on miss -- caller frees */
} bexec_batch_item_t;

/* Runs up to BEXEC_MAX_BATCH commands in one privileged round-trip.
 * Returns 0 on success (check individual items[i].result for NULL on a
 * per-command miss), -1 if the batch itself failed (over budget, exec
 * failure) -- in that case all items[i].result are NULL. */
int bexec_batch(bexec_batch_item_t *items, int n_items);

#endif /* DAEMON_BACKEND_EXEC_H */
