#include "../core/log_safe.h"
#include "daemon_common.h"
/* put shared helpers here if needed; keep minimal for now */
#include <stdio.h>

/* Only one weak attribute is needed */
__attribute__((weak))
const char *device_get_property(const char *key) {
    log_event(LOG_LEVEL_DEBUG, "COMMON", "device_get_property key=%s", key ? key : "(null)");
    return "";
}
