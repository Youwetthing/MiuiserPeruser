#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include "daemon/daemon_core.h"

void daemon_log_info(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    printf("[INFO] ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

void daemon_log_error(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    printf("[ERROR] ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

bool daemon_core_init(const char *daemon_name)
{
    daemon_log_info("starting daemon: %s", daemon_name);
    return true;
}

void daemon_core_shutdown(void)
{
    daemon_log_info("shutdown complete");
}
