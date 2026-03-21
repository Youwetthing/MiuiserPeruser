#ifndef APRIL_EVENT_H
#define APRIL_EVENT_H

#include "sensei_types.h"

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

#endif /* APRIL_EVENT_H */
