#ifndef DAEMON_COMMON_H
#define DAEMON_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

extern int miui_flag_restricted;
extern int thermal_state;

int process_running(const char *name);
int krang_connect(void);
int krang_send_command(const char *cmd);
const char *device_get_property(const char *key);

#ifdef __cplusplus
}
#endif
#endif
