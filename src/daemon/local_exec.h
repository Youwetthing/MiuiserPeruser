/*
 * local_exec.h — Simple command execution, no privilege required
 *
 * Used by resolve_adb_port() to run portscan without routing through
 * bexec() (which would pay the Shizuku round-trip tax).
 */

#ifndef LOCAL_EXEC_H
#define LOCAL_EXEC_H

char *local_exec(const char *cmd);

#endif /* LOCAL_EXEC_H */

