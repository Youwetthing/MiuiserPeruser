#ifndef APRIL_COMPAT_H
#define APRIL_COMPAT_H

#include "april_constants.h"

/* -------------------------
 * Legacy → Core mappings
 * ------------------------- */
#define APRIL_SYSTEM_LOCK   APRIL_KEY_SYSTEM_LOCK
#define APRIL_LOG_LEVEL     APRIL_KEY_LOG_LEVEL

/* legacy SYSLOCK model (temporary shim) */
#define SYSLOCK_NORMAL 0
#define SYSLOCK_LOCKED 1

/* safe sleep helper (legacy wrapper) */
static inline int april_poll_sleep(int base)
{
    if (base < 1) return 1;
    if (base > 10) return 10;
    return base;
}

#endif
