/*
 * truth_engine.c — Krang normalisation / arbitration layer
 *
 * Parses space- or semicolon-separated key=value payloads into a
 * truth_event_t.  Designed to be minimal and allocation-free.
 */

#include "truth_engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ── Internal helpers ─────────────────────────────────────────────────── */

/* Strip leading whitespace; return pointer into s past any spaces/tabs. */
static const char *skip_ws(const char *s)
{
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

/*
 * extract_kv — scan src for the next "key=value" token.
 *
 * Separators between tokens: space, tab, semicolon, comma.
 * A token is split at the first '='.
 * key_out and val_out are NUL-terminated and truncated to their _len limits.
 * Returns pointer to the character after this token, or NULL when exhausted.
 */
static const char *extract_kv(const char *src,
                               char *key_out, size_t key_len,
                               char *val_out, size_t val_len)
{
    src = skip_ws(src);
    if (!src || !*src) return NULL;

    /* advance past separators */
    while (*src == ';' || *src == ',') src++;
    src = skip_ws(src);
    if (!*src) return NULL;

    /* copy key up to '=' */
    size_t ki = 0;
    while (*src && *src != '=' && *src != ' ' && *src != '\t'
                && *src != ';' && *src != ',' && ki < key_len - 1)
        key_out[ki++] = *src++;
    key_out[ki] = '\0';

    if (!*src || *src != '=') {
        /* bare word with no value — skip */
        val_out[0] = '\0';
        return src;
    }
    src++; /* consume '=' */

    /* copy value up to next separator */
    size_t vi = 0;
    while (*src && *src != ' ' && *src != '\t'
                && *src != ';' && *src != ',' && vi < val_len - 1)
        val_out[vi++] = *src++;
    val_out[vi] = '\0';

    return src;
}

/* ── truth_engine_normalise ───────────────────────────────────────────── */

int truth_engine_normalise(const char *raw, truth_event_t *out)
{
    if (!raw || !out) return -1;

    /* Initialise defaults */
    memset(out, 0, sizeof(*out));
    out->voltage = -1;
    out->temp_c  = -1.0f;

    const char *p   = raw;
    int         hits = 0;
    char        key[64], val[256];

    while ((p = extract_kv(p, key, sizeof(key), val, sizeof(val))) != NULL) {
        if (!*key) continue;

        if (strcmp(key, "service") == 0) {
            strncpy(out->service, val, sizeof(out->service) - 1);
            hits++;
        } else if (strcmp(key, "source") == 0) {
            strncpy(out->source, val, sizeof(out->source) - 1);
            hits++;
        } else if (strcmp(key, "level") == 0) {
            out->level = atoi(val);
            hits++;
        } else if (strcmp(key, "voltage") == 0) {
            out->voltage = atoi(val);
            hits++;
        } else if (strcmp(key, "temp_c") == 0 || strcmp(key, "temp") == 0) {
            out->temp_c = (float)atof(val);
            hits++;
        } else if (strcmp(key, "conflict") == 0) {
            out->has_conflict = atoi(val);
            hits++;
        }
        /* Unknown keys are silently ignored */
    }

    /*
     * Conflict detection heuristic:
     *   A level of 0 with a temperature above the WARNING threshold (38°C)
     *   suggests the level field was not updated — mark as conflicted so
     *   krangd can log it for investigation.
     */
    if (!out->has_conflict && out->level == 0 && out->temp_c >= 38.0f)
        out->has_conflict = 1;

    return (hits > 0) ? 0 : -1;
}
