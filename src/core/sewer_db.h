#ifndef SEWER_DB_H
#define SEWER_DB_H

/*
 * sewer_db — splinterd's private accumulation/rollup store.
 * Only splinterd ever opens this. No payload data is ever stored
 * (data-minimization principle from GDPR/sewer_db_policy.md).
 */

int  sewer_db_init(const char *path);
void sewer_db_close(void);

/* Records one inbound emission. detail may be NULL — never pass ev->payload here. */
void sewer_db_record_emit(const char *source, const char *event_type, const char *detail);

#endif
