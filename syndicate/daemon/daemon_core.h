#pragma once
#include <stdbool.h>

void daemon_log_info(const char *fmt, ...);
void daemon_log_error(const char *fmt, ...);

bool daemon_core_init(const char *daemon_name);
void daemon_core_shutdown(void);
