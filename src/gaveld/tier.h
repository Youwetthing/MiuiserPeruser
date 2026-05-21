#ifndef GAVELD_TIER_H
#define GAVELD_TIER_H

/*
 * tier.h — source tier lookup
 * Determines trust modifier for a given source process/package.
 * Ported from scored.c get_tier_modifier().
 *
 * Returns:
 *   TIER_MOD_OWN_DAEMON   (0.40) — MiuiserPeruser own processes
 *   TIER_MOD_SOVEREIGNTY  (0.15) — user-whitelisted apps
 *   TIER_MOD_MIUI_AOSP    (0.60) — system prefixes
 *   TIER_MOD_UNKNOWN      (1.00) — everything else
 */

double tier_modifier(const char *source);

/* Returns 1 if source is a MiuiserPeruser own daemon */
int tier_is_own_daemon(const char *source);

/* Returns 1 if source is on the sovereignty list */
int tier_is_sovereignty(const char *source);

/* Returns 1 if source matches a MIUI/AOSP prefix */
int tier_is_system(const char *source);

#endif /* GAVELD_TIER_H */
