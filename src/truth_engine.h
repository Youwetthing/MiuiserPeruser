/*
 * truth_engine.h — normalise and arbitrate raw key=value payloads
 *
 * Parses space/newline-separated key=value strings from the exec bridge,
 * fills a truth_event_t, and flags conflicts when the same field arrives
 * from multiple sources with disagreeing values.
 *
 * Source authority order (highest first):
 *   rish > dumpsys > adb > sysfs > termux > unknown
 */

#pragma once

#include <stddef.h>

/* ── Event struct ─────────────────────────────────────────────────────── */

typedef struct {
    char    service[64];    /* originating daemon/service name          */
    char    source[32];     /* backend that produced the data           */
    int     level;          /* 0=LOW 1=MEDIUM 2=HIGH 3=CRITICAL         */
    int     voltage;        /* battery voltage in mV  (-1 = unknown)    */
    float   temp_c;         /* temperature in °C      (-999 = unknown)  */
    int     has_conflict;   /* 1 if any field had disagreeing values    */
} truth_event_t;

/* ── API ──────────────────────────────────────────────────────────────── */

/*
 * truth_engine_normalise()
 *
 * Parse a raw "key=value key=value …" string into ev.
 * Handles duplicate keys by keeping the value from the higher-authority
 * source; sets ev->has_conflict if a lower-authority value disagrees
 * beyond the per-field tolerance.
 *
 * Returns  0 on success.
 * Returns -1 if raw is NULL/empty or contains no recognisable fields.
 */
int truth_engine_normalise(const char *raw, truth_event_t *ev);

/*
 * truth_engine_source_priority()
 *
 * Returns the authority rank of a source string (higher = more trusted).
 * Used internally; exposed for unit tests.
 */
int truth_engine_source_priority(const char *source);
