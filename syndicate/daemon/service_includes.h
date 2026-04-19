#ifndef SERVICE_INCLUDES_H
#define SERVICE_INCLUDES_H

#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#include "daemon_common.h"
#include "rish_pipe.h"

extern pid_t g_child_pid;
int miuiserperuser_main_loop(bool foreground);

#endif // SERVICE_INCLUDES_H
