#ifndef GAVELD_AUDIT_H
#define GAVELD_AUDIT_H

/*
 * audit.h — internal affairs audit loop
 *
 * Runs every AUDIT_INTERVAL_SEC (300s) independently.
 * Also handles IA_REFERRAL verdicts from verdict_thread.
 *
 * Checks:
 *   1. Verdict rate  — >30% JAILED in last hour → flag
 *   2. Score inflation — avg score > 60.0 → flag
 *   3. Stall detection — cases PENDING_JUDGEMENT > 5min → flag
 *   4. Repeat flags  — same check flagged 3+ times in 1h → notify
 *   5. IA_REFERRAL   — own daemon flagged, apply softer review
 */

int  audit_start(void);
void audit_stop(void);

/* Signal audit thread to check IA_REFERRALs immediately */
void audit_signal_ia_referral(void);

#endif /* GAVELD_AUDIT_H */
