#ifndef GAVELD_INGEST_H
#define GAVELD_INGEST_H

/*
 * ingest.h — pipes/ingest.pipe reader
 *
 * Reads newline-delimited records from the ingest FIFO:
 *   SOURCE|SIGNAL|WEIGHT|CONTEXT\n
 *
 * Parsed records are pushed onto an internal queue consumed by
 * the scorer thread via ingest_dequeue().
 *
 * Special commands (for control socket passthrough):
 *   QUERY|STATUS\n   — scorer returns full status dump
 *   QUERY|DECAY\n    — triggers immediate decay tick
 */

#include <stddef.h>

#define INGEST_SOURCE_MAX  256
#define INGEST_SIGNAL_MAX   64
#define INGEST_CTX_MAX     512

typedef struct {
    char   source[INGEST_SOURCE_MAX];
    char   signal[INGEST_SIGNAL_MAX];
    double weight;           /* 0 = use weights table */
    char   ctx[INGEST_CTX_MAX];
    int    is_query;         /* 1 = QUERY record, not a signal */
    char   query_cmd[32];    /* STATUS | DECAY */
} ingest_record_t;

/* Start ingest thread — opens FIFO, begins reading */
int  ingest_start(void);
void ingest_stop(void);

/*
 * ingest_dequeue — blocking pop for scorer thread.
 * Fills *out and returns 1 on success.
 * Returns 0 if ingest is stopped and queue is empty.
 */
int ingest_dequeue(ingest_record_t *out);

/* Current queue depth — for monitoring */
int ingest_queue_depth(void);

#endif /* GAVELD_INGEST_H */
