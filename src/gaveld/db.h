#ifndef GAVELD_DB_H
#define GAVELD_DB_H

#include <sqlite3.h>
#include <time.h>

/* Opaque handle — all DB access goes through these functions */
int  db_open(void);
void db_close(void);

/* Schema init — idempotent, safe to call on existing DB */
int  db_init_schema(void);

/* threat_scores */
typedef struct {
    char   source[256];
    double score;
    char   state[32];
    int    prior_jails;
    int    prior_quarantines;
    time_t last_updated;
} db_threat_t;

int  db_threat_load(const char *source, db_threat_t *out);
int  db_threat_upsert(const db_threat_t *rec);
int  db_threat_all(db_threat_t *out, int max, int *count);

/* signal_window */
int  db_signal_insert(const char *source, const char *signal, time_t epoch);
int  db_signal_prune(time_t cutoff);
int  db_signal_distinct(const char *source, time_t since,
                         char signals[16][64], int *count);
int  db_signal_active(const char *source, time_t since);

/* cases */
typedef struct {
    char   case_id[32];
    char   source[256];
    char   signal[64];
    double score;
    char   context[512];
    char   status[32];
    time_t created;
} db_case_t;

int  db_case_insert(const db_case_t *c);
int  db_case_update_status(const char *case_id, const char *status);

/* verdicts */
typedef struct {
    char   case_id[32];
    char   source[256];
    char   verdict[32];
    double score;
    time_t epoch;
    int    consent_required;
    int    consent_granted;
    char   mitre_id[16];
    char   mitre_tactic[32];
} db_verdict_t;

int  db_verdict_insert(const db_verdict_t *v);
int  db_verdict_count_recent(const char *source, const char *verdict,
                              time_t since);
int  db_verdict_count_all(time_t since);
int  db_verdict_count_by_type(const char *verdict, time_t since);

/* criminal_record */
int  db_criminal_record_insert(const char *source, const char *verdict,
                                const char *reason, const char *actor,
                                time_t epoch);

/* scoring_log */
int  db_scoring_log_insert(time_t epoch, const char *source,
                            const char *signal, double base_weight,
                            double final_addition, double prev_score,
                            double new_score, const char *state,
                            const char *modifiers);

/* consent_queue */
typedef struct {
    char   case_id[32];
    char   source[256];
    char   verdict[32];
    double score;
    time_t queued;
    int    timeout_secs;
    char   timeout_action[16];
} db_consent_t;

int  db_consent_insert(const db_consent_t *c);
int  db_consent_remove(const char *case_id);
int  db_consent_pending(db_consent_t *out, int max, int *count);

/* audit_log */
int  db_audit_log_insert(time_t epoch, const char *check,
                          const char *result, const char *detail);
int  db_audit_flag_count(const char *check, time_t since);

/* WAL checkpoint */
void db_checkpoint(void);

#endif /* GAVELD_DB_H */
