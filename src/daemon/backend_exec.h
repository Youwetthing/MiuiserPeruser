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

/*
 * Outcome of the most recent bexec()/bexec_n() call.
 * A daemon that only needs "did this work" can rely on the NULL return;
 * one that needs to tell "command ran and said nothing" apart from
 * "could not run the command at all" reads bexec_last_status().
 */
typedef enum {
    BEXEC_OK          =  0,  /* command ran, exited 0                    */
    BEXEC_ERR_EXIT    = -1,  /* command ran, exited non-zero, no output  */
    BEXEC_ERR_SPAWN   = -2,  /* could not start the command at all       */
    BEXEC_ERR_TIMEOUT = -3,  /* killed on timeout with no output         */
    BEXEC_ERR_NOMEM   = -4,  /* allocation failure                       */
} BEXEC_STATUS;

BEXEC_STATUS  bexec_last_status(void);
const char   *bexec_status_str(BEXEC_STATUS s);

void          bexec_init(void);
void          backend_reprobe(void);
BACKEND_TYPE  backend_get(void);
const char   *backend_name(BACKEND_TYPE b);
BACKEND_TYPE  splinter_get_backend(void);

/*
 * Both return a malloc'd string the caller must free().
 *
 * NULL means the command could not be run, or ran and failed without
 * producing output (see bexec_last_status()). A non-NULL empty string
 * means the command ran successfully and produced no output. Callers
 * must not conflate the two: an empty result from a dead backend used
 * to read as a confirmed-clean measurement.
 */
char *bexec(const char *cmd);
char *bexec_n(const char *cmd, size_t maxout);
char *bexec_read_file(const char *path);

#endif /* DAEMON_BACKEND_EXEC_H */
