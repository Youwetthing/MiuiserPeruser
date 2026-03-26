#ifndef SUPERHERO_MODE_H
#define SUPERHERO_MODE_H

#include <stdint.h>
#include "sensei_types.h"

/* Run a single full scan and exit. */
SENSEI_STATUS superhero_run_once(void);

/* Run full scans forever, every interval_seconds. */
SENSEI_STATUS superhero_run_loop(uint32_t interval_seconds);

/* Run full scans exactly 'count' times, with interval_seconds between scans. */
SENSEI_STATUS superhero_run_n_times(uint32_t count, uint32_t interval_seconds);

#endif /* SUPERHERO_MODE_H */
