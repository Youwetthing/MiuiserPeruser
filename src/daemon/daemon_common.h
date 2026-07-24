#ifndef DAEMON_COMMON_H
#define DAEMON_COMMON_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int miui_flag_restricted;
extern int thermal_state;

int process_running(const char *name);
int krang_connect(void);
int krang_send_command(const char *cmd);
const char *device_get_property(const char *key);

/*
 * Results-file helpers.
 *
 * A daemon's JSON results file is its only output to the rest of the
 * system, so a failed open or a short write must be visible: a reader
 * that finds a stale file otherwise has no way to tell it apart from a
 * fresh one. results_open() logs why the open failed; results_close()
 * flushes, checks the stream error flag, and logs a truncated write.
 *
 * results_close() returns 0 on success, -1 if anything went wrong.
 */
FILE *results_open(const char *daemon, const char *path);
int   results_close(const char *daemon, const char *path, FILE *f);

/*
 * splinter_emit — send one "APRIL|<daemon>|<type>|<payload>" line to
 * splinterd's UNIX socket.
 *
 * Shared by every daemon that used to open-code this with the connect()
 * result and the write() return value both discarded, so an event that
 * never reached splinterd looked exactly like one that did. Returns 0 on
 * success, -1 otherwise, and reports the first failure per process
 * (rearmed on the next success) rather than one line per poll.
 */
int splinter_emit(const char *daemon, const char *socket_path,
                  const char *type, const char *payload);

#ifdef __cplusplus
}
#endif
#endif
