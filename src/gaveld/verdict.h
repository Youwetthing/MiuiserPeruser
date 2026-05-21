#ifndef GAVELD_VERDICT_H
#define GAVELD_VERDICT_H

/*
 * verdict.h — 5-state verdict machine
 *
 * Consumes cases from cases_dequeue(), applies:
 *   - IA lock check
 *   - Own daemon → IA referral
 *   - Sovereignty cap
 *   - Ethical floors
 *   - WARNED → immediate enforce
 *   - QUARANTINED/HOUSE_ARREST/JAILED → consent queue (async)
 *
 * Runs as a pthread. consent.c handles the async approval flow
 * and calls enforce directly on approval.
 */

/* Start/stop verdict thread */
int  verdict_start(void);
void verdict_stop(void);

/*
 * verdict_emit — write verdict to DB and criminal_record.
 * Called by both verdict_thread (WARNED) and consent_thread (others).
 */
void verdict_emit(const char *case_id, const char *source,
                  const char *verdict, double score,
                  int consent_required, int consent_granted);

#endif /* GAVELD_VERDICT_H */
