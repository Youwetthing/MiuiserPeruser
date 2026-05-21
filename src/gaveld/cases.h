#ifndef GAVELD_CASES_H
#define GAVELD_CASES_H

/*
 * cases.h — case assembly
 * Replaces cre/cases/ JSON files and april_o_neil.sh case routing.
 *
 * Flow:
 *   scorer_process() → cases_assemble() → DB insert → verdict queue
 *
 * CLEAN and WATCHED verdicts are logged but not enqueued for verdict_thread.
 * Everything else is pushed to the verdict queue.
 */

#include "scorer.h"

#define CASE_ID_MAX  32
#define CASES_DB_CAP 500   /* max cases in DB before eviction */

typedef struct {
    char   case_id[CASE_ID_MAX];
    char   source[256];
    char   signal[64];
    char   state[32];
    double score;
    double delta;
    char   ctx[512];
    long   epoch;
} case_record_t;

/*
 * cases_assemble — build a case from a score result and enqueue for verdict.
 * Returns 0 on success, -1 on error.
 */
int cases_assemble(const score_result_t *result,
                   const char *signal,
                   const char *ctx);

/*
 * cases_dequeue — blocking pop for verdict_thread.
 * Returns 1 on success, 0 if stopped and empty.
 */
int cases_dequeue(case_record_t *out);

/* Signal stop — wakes any blocked dequeue */
void cases_stop(void);

/* Current queue depth */
int cases_queue_depth(void);

#endif /* GAVELD_CASES_H */
