#ifndef MIUISERPERUSER_IPC_H
#define MIUISERPERUSER_IPC_H

#include "../core/include/sensei_types.h"
#include <stdbool.h>

SENSEI_STATUS miuiserperuser_ipc_init(void);
void miuiserperuser_ipc_shutdown(void);
void miuiserperuser_ipc_broadcast(const SENSEI_DETECTION *detection);
bool miuiserperuser_ipc_is_connected(void);

#endif
