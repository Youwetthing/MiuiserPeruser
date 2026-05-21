#ifndef GAVELD_SCORER_H
#define GAVELD_SCORER_H

/*
 * scorer.h — signal scoring engine
 * Ports score_signal() and covariance matrix from scored.c.
 * Calls db.c for all state — no direct SQLite here.
 */

typedef struct {
    char   source[256];
    double prev_score;
    double new_score;
    double delta;
    char   state[32];
    int    prior_jails;
    int    prior_quarantines;
} score_result_t;

/*
 * scorer_process — score an incoming signal for a source.
 *
 * source      — process/package name
 * signal      — signal type (looked up in weights.c)
 * base_weight — weight from ingest (0 = use weights table)
 * ctx         — context string, stored in scoring_log
 * out         — populated with result
 *
 * Returns 0 on success, -1 on DB error.
 */
int scorer_process(const char *source, const char *signal,
                   double base_weight, const char *ctx,
                   score_result_t *out);

/* Format result as wire string: SOURCE|SCORE|STATE|DELTA\n */
void scorer_format(const score_result_t *r, char *buf, int len);

#endif /* GAVELD_SCORER_H */
