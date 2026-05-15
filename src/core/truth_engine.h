/*
 * truth_engine.h — Krang normalisation layer
 *
 * Parses raw key=value payloads forwarded to krangd (e.g. thermal reports,
 * status blobs) into a canonical truth_event_t.  Arbitrates conflicting
 * readings and sets has_conflict when sources disagree.
 *
 * Wire format expected by truth_engine_normalise():
 *   key=value pairs separated by spaces or semicolons, e.g.
 *   "service=thermals source=leatherhead level=2 voltage=4100 temp_c=38.5"
 */

#ifndef TRUTH_ENGINE_H
#define TRUTH_ENGINE_H

#ifdef __cplusplus
extern "C" {
#endif

/* ── Canonical event ──────────────────────────────────────────────────── */

typedef struct {
    char  service[64];   /* originating service name, e.g. "thermals"   */
    char  source[64];    /* daemon that emitted the reading, e.g. "leatherhead" */
    int   level;         /* severity / alert level (0 = nominal)         */
    int   voltage;       /* battery voltage in mV (-1 = absent)          */
    float temp_c;        /* temperature in °C   (-1.0 = absent)          */
    int   has_conflict;  /* 1 if two sources disagree on level/temp       */
} truth_event_t;

/* ── API ──────────────────────────────────────────────────────────────── */

/*
 * truth_engine_normalise — parse raw key=value string into truth_event_t.
 *
 * Returns  0 on success.
 * Returns -1 if raw is NULL/empty or contains no recognisable fields.
 *
 * Unknown keys are silently ignored; missing fields keep their zero/−1
 * defaults so callers can always safely dereference the result.
 */
int truth_engine_normalise(const char *raw, truth_event_t *out);

#ifdef __cplusplus
}
#endif

#endif /* TRUTH_ENGINE_H */
