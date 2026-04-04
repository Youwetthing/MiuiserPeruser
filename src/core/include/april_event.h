#ifndef APRIL_EVENT_H
#define APRIL_EVENT_H

#include "sensei_types.h"

/* Existing event queue API */
SENSEI_STATUS april_event_queue_init(SENSEI_EVENT_QUEUE *queue);
void april_event_queue_destroy(SENSEI_EVENT_QUEUE *queue);
SENSEI_STATUS april_event_queue_push(SENSEI_EVENT_QUEUE *queue,
                                     const SENSEI_DETECTION *detection);
SENSEI_STATUS april_event_queue_pop(SENSEI_EVENT_QUEUE *queue,
                                    SENSEI_DETECTION *detection,
                                    int timeout_ms);
void april_event_register_callback(SENSEI_EVENT_PRIORITY priority,
                                   void (*callback)(const SENSEI_DETECTION*, void*),
                                   void *user_data);

/* Unified lightweight event bus for daemons / CLIs */

typedef enum _APRIL_EVENT_TYPE {
    APRIL_EVENT_BACKEND_SELECTED = 0,
    APRIL_EVENT_SCAN_START,
    APRIL_EVENT_SCAN_END,
    APRIL_EVENT_METRIC_THERMAL,
    APRIL_EVENT_METRIC_BATTERY,
    APRIL_EVENT_METRIC_CPUFREQ
} APRIL_EVENT_TYPE;

/*
 * Emit a simple event to stdout.
 *
 * If APRIL_EVENT_JSON is set and not "0", events are printed as JSON:
 *   {"event":"thermal","value":42000}
 *
 * Otherwise, a simple tagged line is printed:
 *   [THERMAL] 42000
 */
void april_emit_event(APRIL_EVENT_TYPE type, int value);

#endif /* APRIL_EVENT_H */
