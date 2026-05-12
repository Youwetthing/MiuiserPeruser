#ifndef MIUISERPERUSER_DAEMON_CORE_H
#define MIUISERPERUSER_DAEMON_CORE_H

#include <stdbool.h>

int miuiserperuser_ipc_init(void);
void miuiserperuser_ipc_shutdown(void);
int miuiserperuser_main_loop(bool console_mode);

#endif
