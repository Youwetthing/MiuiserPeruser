#pragma once

#include <stdbool.h>

/*
 * Shared lifecycle for all TMNT daemons.
 * Handles:
 *  - startup banner
 *  - logging wrappers
 *  - signal handling
 *  - PID file creation
 *  - runtime directory creation
 */

bool daemon_core_init(const char *daemon_name);
void daemon_core_shutdown(void);

/* Logging wrappers */
void daemon_log_info(const char *fmt, ...);
void daemon_log_error(const char *fmt, ...);
void daemon_log_debug(const char *fmt, ...);
