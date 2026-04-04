/*
 * MiuiserPeruser – Event queue (April's domain)
 */

#include <april_event.h>
#include <leo_detection.h>
#include <sensei_types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#define RING_BUFFER_SIZE 1024

typedef struct _callback_entry {
    SENSEI_EVENT_PRIORITY priority;
    void (*callback)(const SENSEI_DETECTION*, void*);
    void *user_data;
    struct _callback_entry *next;
} callback_entry_t;

static callback_entry_t *g_callbacks = NULL;
static pthread_mutex_t g_callback_mutex = PTHREAD_MUTEX_INITIALIZER;

SENSEI_STATUS april_event_queue_init(SENSEI_EVENT_QUEUE *queue) {
    if (!queue) return SENSEI_STATUS_ERROR;
    memset(queue, 0, sizeof(SENSEI_EVENT_QUEUE));
    queue->running = true;
    return SENSEI_STATUS_OK;
}

void april_event_queue_destroy(SENSEI_EVENT_QUEUE *queue) {
    if (!queue) return;
    queue->running = false;
    for (int i = 0; i < SENSEI_EVENT_PRIORITY_COUNT; i++) {
        SENSEI_DETECTION *cur = queue->queues[i].head;
        while (cur) {
            SENSEI_DETECTION *next = cur->next;
            free(cur);
            cur = next;
        }
        queue->queues[i].head = queue->queues[i].tail = NULL;
        queue->queues[i].count = 0;
    }
    queue->total_count = 0;
}

SENSEI_STATUS april_event_queue_push(SENSEI_EVENT_QUEUE *queue,
                                     const SENSEI_DETECTION *detection) {
    if (!queue || !detection || !queue->running) return SENSEI_STATUS_ERROR;

    int idx = detection->priority;
    if (idx < 0 || idx >= SENSEI_EVENT_PRIORITY_COUNT)
        idx = SENSEI_EVENT_PRIORITY_MEDIUM;

    SENSEI_DETECTION *copy = malloc(sizeof(SENSEI_DETECTION));
    if (!copy) return SENSEI_STATUS_NO_MEMORY;
    memcpy(copy, detection, sizeof(SENSEI_DETECTION));
    copy->next = NULL;

    SENSEI_DETECTION_LIST *q = &queue->queues[idx];
    if (!q->head) {
        q->head = q->tail = copy;
    } else {
        q->tail->next = copy;
        q->tail = copy;
    }
    q->count++;
    queue->total_count++;
    return SENSEI_STATUS_OK;
}

SENSEI_STATUS april_event_queue_pop(SENSEI_EVENT_QUEUE *queue,
                                    SENSEI_DETECTION *detection,
                                    int timeout_ms) {
    if (!queue || !detection) return SENSEI_STATUS_ERROR;
    (void)timeout_ms; /* non-blocking for now */

    for (int i = 0; i < SENSEI_EVENT_PRIORITY_COUNT; i++) {
        if (queue->queues[i].head) {
            SENSEI_DETECTION *node = queue->queues[i].head;
            memcpy(detection, node, sizeof(SENSEI_DETECTION));
            queue->queues[i].head = node->next;
            if (!queue->queues[i].head)
                queue->queues[i].tail = NULL;
            queue->queues[i].count--;
            queue->total_count--;
            free(node);
            return SENSEI_STATUS_OK;
        }
    }
    return SENSEI_STATUS_NOT_FOUND;
}

void april_event_register_callback(SENSEI_EVENT_PRIORITY priority,
                                   void (*callback)(const SENSEI_DETECTION*, void*),
                                   void *user_data) {
    pthread_mutex_lock(&g_callback_mutex);
    callback_entry_t *entry = malloc(sizeof(callback_entry_t));
    if (!entry) {
        pthread_mutex_unlock(&g_callback_mutex);
        return;
    }
    entry->priority = priority;
    entry->callback = callback;
    entry->user_data = user_data;
    entry->next = g_callbacks;
    g_callbacks = entry;
    pthread_mutex_unlock(&g_callback_mutex);
}

/* Internal: dispatch events to registered callbacks (called by the main loop) */
void april_dispatch_events(SENSEI_EVENT_QUEUE *queue) {
    SENSEI_DETECTION detection;
    while (april_event_queue_pop(queue, &detection, 0) == SENSEI_STATUS_OK) {
        pthread_mutex_lock(&g_callback_mutex);
        callback_entry_t *cur = g_callbacks;
        while (cur) {
            if (cur->priority <= detection.priority)
                cur->callback(&detection, cur->user_data);
            cur = cur->next;
        }
        pthread_mutex_unlock(&g_callback_mutex);
    }
}

/* ---- Unified lightweight event emitter for daemons / CLIs ---- */

static const char *april_event_name(APRIL_EVENT_TYPE type) {
    switch (type) {
    case APRIL_EVENT_BACKEND_SELECTED: return "backend_selected";
    case APRIL_EVENT_SCAN_START:       return "scan_start";
    case APRIL_EVENT_SCAN_END:         return "scan_end";
    case APRIL_EVENT_METRIC_THERMAL:   return "thermal";
    case APRIL_EVENT_METRIC_BATTERY:   return "battery";
    case APRIL_EVENT_METRIC_CPUFREQ:   return "cpu_freq";
    default:                           return "unknown";
    }
}

void april_emit_event(APRIL_EVENT_TYPE type, int value) {
    const char *name = april_event_name(type);
    const char *json = getenv("APRIL_EVENT_JSON");

    if (json && json[0] != '\0' && strcmp(json, "0") != 0) {
        /* JSON mode */
        printf("{\"event\":\"%s\",\"value\":%d}\n", name, value);
    } else {
        /* Simple tagged line */
        printf("[%s] %d\n", name, value);
    }
    fflush(stdout);
}
