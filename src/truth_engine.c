/*
 * truth_engine.c — normalise and arbitrate raw key=value payloads
 */

#include "truth_engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

/* ── Tolerances for conflict detection ───────────────────────────────── */

#define VOLTAGE_CONFLICT_MV   50      /* >50 mV difference = conflict    */
#define TEMP_CONFLICT_C       2.0f    /* >2.0°C difference = conflict    */

/* ── Defaults ─────────────────────────────────────────────────────────── */

#define DEFAULT_VOLTAGE  -1
#define DEFAULT_TEMP_C   -999.0f
#define DEFAULT_LEVEL    0

/* ── Source authority ─────────────────────────────────────────────────── */

int truth_engine_source_priority(const char *source)
{
    if (!source || !*source)    return 0;
    if (strstr(source, "rish"))    return 5;
    if (strstr(source, "dumpsys")) return 4;
    if (strstr(source, "adb"))     return 3;
    if (strstr(source, "sysfs"))   return 2;
    if (strstr(source, "termux"))  return 1;
    return 0;
}

/* ── Internal parser state ────────────────────────────────────────────── */

typedef struct {
    truth_event_t *ev;

    /* track how each numeric field was last set so we can arbitrate */
    int   voltage_set;       /* 1 = already set */
    int   voltage_prio;      /* priority of the source that set it  */
    int   temp_set;
    int   temp_prio;
    int   level_set;
    int   level_prio;

    int   fields_seen;       /* count of recognised fields parsed   */
} parse_state_t;

/* ── Per-field setters with conflict arbitration ─────────────────────── */

static void set_service(parse_state_t *st, const char *val)
{
    strncpy(st->ev->service, val, sizeof(st->ev->service) - 1);
    st->ev->service[sizeof(st->ev->service) - 1] = '\0';
    st->fields_seen++;
}

static void set_source(parse_state_t *st, const char *val)
{
    strncpy(st->ev->source, val, sizeof(st->ev->source) - 1);
    st->ev->source[sizeof(st->ev->source) - 1] = '\0';
    st->fields_seen++;
}

static void set_level(parse_state_t *st, int val, int prio)
{
    /* clamp to [0, 3] */
    if (val < 0) val = 0;
    if (val > 3) val = 3;

    if (!st->level_set) {
        st->ev->level = val;
        st->level_set = 1;
        st->level_prio = prio;
    } else if (val != st->ev->level) {
        st->ev->has_conflict = 1;
        if (prio > st->level_prio) {
            st->ev->level  = val;
            st->level_prio = prio;
        }
    }
    st->fields_seen++;
}

static void set_voltage(parse_state_t *st, int val, int prio)
{
    if (!st->voltage_set) {
        st->ev->voltage  = val;
        st->voltage_set  = 1;
        st->voltage_prio = prio;
    } else if (abs(val - st->ev->voltage) > VOLTAGE_CONFLICT_MV) {
        st->ev->has_conflict = 1;
        if (prio > st->voltage_prio) {
            st->ev->voltage  = val;
            st->voltage_prio = prio;
        }
    }
    st->fields_seen++;
}

static void set_temp(parse_state_t *st, float val, int prio)
{
    if (!st->temp_set) {
        st->ev->temp_c = val;
        st->temp_set   = 1;
        st->temp_prio  = prio;
    } else if (fabsf(val - st->ev->temp_c) > TEMP_CONFLICT_C) {
        st->ev->has_conflict = 1;
        if (prio > st->temp_prio) {
            st->ev->temp_c = val;
            st->temp_prio  = prio;
        }
    }
    st->fields_seen++;
}

/* ── Token parser ─────────────────────────────────────────────────────── */

/*
 * Process one key=value token.
 * The source priority is derived from the source field seen so far in
 * the same payload; caller resolves this after all tokens are parsed by
 * doing a two-pass approach, but for simplicity we re-derive priority
 * from ev->source which gets updated as we go.
 */
static void apply_token(parse_state_t *st, const char *key, const char *val)
{
    int prio = truth_engine_source_priority(st->ev->source);

    if (strcmp(key, "service") == 0) {
        set_service(st, val);
    } else if (strcmp(key, "source") == 0) {
        set_source(st, val);
        /* re-derive priority now that source is known */
        (void)prio;
    } else if (strcmp(key, "level") == 0) {
        set_level(st, atoi(val), prio);
    } else if (strcmp(key, "voltage") == 0 || strcmp(key, "voltage_mv") == 0) {
        set_voltage(st, atoi(val), prio);
    } else if (strcmp(key, "temp") == 0    ||
               strcmp(key, "temp_c") == 0  ||
               strcmp(key, "temperature") == 0) {
        set_temp(st, strtof(val, NULL), prio);
    }
    /* unknown keys are silently ignored */
}

/* ── Public API ───────────────────────────────────────────────────────── */

int truth_engine_normalise(const char *raw, truth_event_t *ev)
{
    if (!raw || !ev) return -1;

    /* Zero-initialise with sentinel unknowns */
    memset(ev, 0, sizeof(*ev));
    ev->voltage = DEFAULT_VOLTAGE;
    ev->temp_c  = DEFAULT_TEMP_C;
    ev->level   = DEFAULT_LEVEL;

    parse_state_t st;
    memset(&st, 0, sizeof(st));
    st.ev = ev;

    /* Work on a mutable copy */
    char *buf = strdup(raw);
    if (!buf) return -1;

    char *p = buf;
    while (*p) {
        /* Skip whitespace / separators */
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ';')
            p++;
        if (!*p) break;

        /* Find key */
        char *key_start = p;
        while (*p && *p != '=' && *p != ' ' && *p != '\t')
            p++;

        if (*p != '=') {
            /* No '=' found — skip this token */
            while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
            continue;
        }

        *p++ = '\0';   /* null-terminate the key */

        /* Find value — may be quoted */
        char *val_start;
        if (*p == '"') {
            p++;
            val_start = p;
            while (*p && *p != '"') p++;
            if (*p == '"') *p++ = '\0';
        } else {
            val_start = p;
            while (*p && *p != ' ' && *p != '\t' && *p != '\n'
                       && *p != '\r' && *p != ';')
                p++;
            if (*p) *p++ = '\0';
        }

        /* Lowercase key for case-insensitive matching */
        for (char *k = key_start; *k; k++)
            *k = (char)tolower((unsigned char)*k);

        apply_token(&st, key_start, val_start);
    }

    free(buf);

    if (st.fields_seen == 0) return -1;
    return 0;
}
