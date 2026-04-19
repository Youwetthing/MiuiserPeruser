#ifndef DAEMON_CORE_H
#define DAEMON_CORE_H

#include <stdarg.h>

void daemon_log_info(const char *fmt, ...);
void daemon_log_error(const char *fmt, ...);

int  daemon_core_init(const char *name);
void daemon_core_shutdown(void);

#endif
