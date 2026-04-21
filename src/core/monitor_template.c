#include <unistd.h>
#include <stdint.h>

#include "april_runtime.h"
#include "april_constants.h"

/* -------------------------
 * Monitor config structure
 * ------------------------- */
typedef struct {
    int base_sleep;
} april_monitor_cfg;

/* -------------------------
 * Main monitor loop
 * ------------------------- */
void april_monitor_run(april_monitor_cfg *cfg)
{
    while (1)
    {
        /* system lock check */
        uint32_t syslock =
            april_read(APRIL_KEY_SYSTEM_LOCK, POLL_NORMAL);

        if (syslock == 1)
        {
            /* locked state → back off */
            sleep(1);
            continue;
        }

        /* log level read */
        uint32_t log_level =
            april_read(APRIL_KEY_LOG_LEVEL, POLL_NORMAL);

        (void)log_level; /* placeholder for future logging logic */

        /* throttle / base sleep */
        sleep((unsigned int)cfg->base_sleep);
    }
}
