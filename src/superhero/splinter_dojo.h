#ifndef SPLINTER_DOJO_H
#define SPLINTER_DOJO_H

#include "../core/include/sensei_types.h"
#include "backend_strategy.h"

SENSEI_STATUS splinter_init(void);
SENSEI_STATUS splinter_run_scan_cycle(uint32_t interval_ms);
SENSEI_STATUS splinter_collect_metrics(splinter_metrics_t *out);
void          splinter_shutdown(void);
BACKEND_TYPE  splinter_get_backend(void);
void          splinter_set_depth(const char *depth);

#endif
