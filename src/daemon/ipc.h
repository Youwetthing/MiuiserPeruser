#ifndef MIUISERPERUSER_IPC_H
#define MIUISERPERUSER_IPC_H

#include "../core/include/sensei_types.h"
#include <stdbool.h>

SENSEI_STATUS miuiserperuser_ipc_init(void);
void miuiserperuser_ipc_shutdown(void);
void miuiserperuser_ipc_broadcast(const SENSEI_DETECTION *detection);
bool miuiserperuser_ipc_is_connected(void);

#endif
#define DAEMONHUNTER_SOCKET 4
#define SEWER_SOCKET 3
void ipc_init(void);
char* ipc_recv_msg(int socket_id);
void ipc_send_msg(int socket_id, const char* msg);
