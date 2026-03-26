#ifndef SPLINTER_DOJO_H
#define SPLINTER_DOJO_H

#include <stdint.h>
#include "sensei_types.h"

/* Initialize Splinter’s orchestration layer.
 * This will prepare the detection engine (Leo + turtles).
 */
SENSEI_STATUS splinter_init(void);

/* Run a single full scan cycle:
 * - consult Sensei knowledge (future hook)
 * - trigger Leo full scan (turtles roll out)
 * - hand results to April for logging/formatting
 *
 * interval_ms is advisory (for logging / cadence hints).
 * The caller is still responsible for actual sleeping.
 */
SENSEI_STATUS splinter_run_scan_cycle(uint32_t interval_ms);

/* Shutdown Splinter orchestration cleanly. */
void splinter_shutdown(void);

#endif /* SPLINTER_DOJO_H */
