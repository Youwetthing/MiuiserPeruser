#ifndef SPLINTER_DOJO_H
#define SPLINTER_DOJO_H

#include <stdint.h>
#include "sensei_types.h"
#include "backend_strategy.h"

/*
 * Splinter Dojo – orchestrates backend selection and exposes
 * the chosen backend to Superhero Mode.
 */

typedef struct {
    int thermal;
    int battery;
    int cpu_freq;
} splinter_metrics_t;

SENSEI_STATUS splinter_init(void);
SENSEI_STATUS splinter_run_scan_cycle(uint32_t interval_ms);
SENSEI_STATUS splinter_collect_metrics(splinter_metrics_t *out);
void          splinter_shutdown(void);

BACKEND_TYPE  splinter_get_backend(void);

#endif /* SPLINTER_DOJO_H */
