#ifndef DAEMON_COMMON_H
#define DAEMON_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

/* Shared state flags — set by detection daemons, readable system-wide. */
extern int miui_flag_restricted;
extern int thermal_state;

/* Returns 1 if a process with the given name is running, 0 otherwise. */
int process_running(const char *name);

/* Weak-symbol property reader — returns "" when native getprop not available. */
const char *device_get_property(const char *key);

#ifdef __cplusplus
}
#endif
#endif /* DAEMON_COMMON_H */
