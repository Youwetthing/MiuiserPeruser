#ifndef GAVELD_ENFORCE_H
#define GAVELD_ENFORCE_H

/*
 * enforce.h — rish/adb execution backend
 *
 * Tier-gated enforcement:
 *   Sovereignty / MIUI/AOSP  → am force-stop only, always
 *   Unknown / third-party    → am force-stop for QUARANTINED/HOUSE_ARREST
 *                           → am force-stop + pm disable-user for JAILED
 *
 * Probes for best backend once at startup: rish → adb → direct popen.
 * All callers go through enforce_execute() — never call bexec() directly.
 */

/* Probe and cache best available backend — call once from gaveld.c main() */
void enforce_init(void);

/*
 * enforce_execute — run enforcement action for source/verdict.
 * Returns 0 on success, -1 if enforcement was blocked by tier policy.
 */
int enforce_execute(const char *source, const char *verdict,
                    const char *case_id, double score);

#endif /* GAVELD_ENFORCE_H */
