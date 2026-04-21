#include "april_runtime.h"

/* stub implementation (replace hardware logic later) */
uint32_t april_read(APRIL_KEY key, APRIL_POLL_MODE mode)
{
    switch (key)
    {
        case APRIL_KEY_SYSTEM_LOCK:
            return (mode == POLL_MINIMAL) ? 0 : 1;

        case APRIL_KEY_LOG_LEVEL:
            return (mode == POLL_MINIMAL) ? LOG_NORMAL : LOG_VERBOSE;

        case APRIL_KEY_THROTTLE:
            return 0;

        default:
            return 0;
    }
}
