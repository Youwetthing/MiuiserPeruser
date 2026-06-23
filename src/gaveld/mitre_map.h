#ifndef MITRE_MAP_H
#define MITRE_MAP_H

typedef struct {
    const char *signal_type;
    const char *technique_id;   /* e.g. "T1406" */
    const char *tactic;         /* e.g. "defense-evasion" */
    const char *technique_name; /* e.g. "Obfuscated Files or Information" */
} mitre_entry_t;

const mitre_entry_t *mitre_lookup(const char *signal_type);

#endif /* MITRE_MAP_H */
