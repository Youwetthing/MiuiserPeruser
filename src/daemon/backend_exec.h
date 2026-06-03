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
    BACKEND_RISH   = 0,
    BACKEND_ADB    = 1,
    BACKEND_DIRECT = 2,
} BACKEND_TYPE;

void          bexec_init(void);
void          backend_reprobe(void);
BACKEND_TYPE  backend_get(void);
const char   *backend_name(BACKEND_TYPE b);
BACKEND_TYPE  splinter_get_backend(void);

char *bexec(const char *cmd);
char *bexec_n(const char *cmd, size_t maxout);
char *bexec_read_file(const char *path);

#endif /* DAEMON_BACKEND_EXEC_H */
