#ifndef GAVELD_WEIGHTS_H
#define GAVELD_WEIGHTS_H

/*
 * weights.h — signal weight table
 * Each signal has a base weight and an optional MITRE ATT&CK technique ID.
 * weight_lookup() returns 0 if signal is unknown (caller should default to 10).
 */

typedef struct {
    const char *signal;
    int         base_weight;
    const char *mitre_id;    /* NULL if not mapped */
} weight_entry_t;

/* Returns base weight for signal, or 0 if not found */
int weight_lookup(const char *signal);

/* Returns MITRE ID for signal, or NULL if not mapped */
const char *weight_mitre(const char *signal);

/* Total number of entries in the table */
int weight_table_size(void);

#endif /* GAVELD_WEIGHTS_H */
