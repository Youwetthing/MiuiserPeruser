#ifndef APRIL_CONSTANTS_H
#define APRIL_CONSTANTS_H

#include <stdint.h>

/* -------------------------
 * CORE KEY SYSTEM
 * ------------------------- */
typedef enum {
    APRIL_KEY_THROTTLE     = 0x01,
    APRIL_KEY_SYSTEM_LOCK  = 0x02,
    APRIL_KEY_LOG_LEVEL    = 0x03
} APRIL_KEY;

/* -------------------------
 * POLL MODES
 * ------------------------- */
typedef enum {
    POLL_NORMAL = 0,
    POLL_MINIMAL = 1
} APRIL_POLL_MODE;

/* -------------------------
 * LOG LEVELS (runtime only)
 * ------------------------- */
typedef enum {
    LOG_NORMAL = 0,
    LOG_VERBOSE = 1
} APRIL_LOG_LEVEL;

/* -------------------------
 * DEFAULT BIN PATH (if used)
 * ------------------------- */
#define APRIL_BIN "/data/local/tmp/april.bin"

#endif
