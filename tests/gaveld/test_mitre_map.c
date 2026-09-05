/* Unit tests for src/gaveld/mitre_map.c — ATT&CK Mobile enrichment lookup. */

#include "test_harness.h"
#include "mitre_map.h"

#include <string.h>

static void test_lookup_known_signal(void) {
    const mitre_entry_t *e = mitre_lookup("ROOTKIT_MODULE");
    CHECK(e != NULL);
    if (!e) return;
    CHECK_STR_EQ(e->signal_type, "ROOTKIT_MODULE");
    CHECK_STR_EQ(e->technique_id, "T1406");
    CHECK_STR_EQ(e->tactic, "defense-evasion");
    CHECK_STR_EQ(e->technique_name, "Obfuscated Files or Information");
}

static void test_lookup_across_categories(void) {
    const mitre_entry_t *net = mitre_lookup("DNS_ANOMALY");
    CHECK(net != NULL);
    if (net) CHECK_STR_EQ(net->tactic, "command-and-control");

    const mitre_entry_t *tel = mitre_lookup("MIUI_ANALYTICS");
    CHECK(tel != NULL);
    if (tel) CHECK_STR_EQ(tel->technique_id, "T1643");

    const mitre_entry_t *hook = mitre_lookup("FRIDA_DETECTED");
    CHECK(hook != NULL);
    if (hook) CHECK_STR_EQ(hook->technique_name, "Hooking");
}

static void test_unknown_and_null(void) {
    CHECK(mitre_lookup("NOT_A_SIGNAL") == NULL);
    CHECK(mitre_lookup("") == NULL);
    CHECK(mitre_lookup(NULL) == NULL);
    /* Sentinel row must not be returned as a match. */
    CHECK(mitre_lookup("ROOTKIT_MODULE_") == NULL);
}

static void test_returned_pointer_is_stable(void) {
    const mitre_entry_t *a = mitre_lookup("XPOSED_DETECTED");
    const mitre_entry_t *b = mitre_lookup("XPOSED_DETECTED");
    CHECK(a != NULL && a == b);
}

static const char *MAPPED_SIGNALS[] = {
    "KERNEL_MODULE_UNKNOWN", "ROOTKIT_MODULE", "ANON_RWX_MEM", "INJECTOR_LIB",
    "EXEC_FROM_DATA", "MILLET_ACTIVE", "SUSPECT_PORT", "ADB_TCP_ACTIVE",
    "DNS_ANOMALY", "CONNECTIVITY_DRIFT", "UNKNOWN_LISTENER",
    "PRIVATE_DNS_INACTIVE", "FB_PARTNER_ID", "PARTNER_TOKEN", "SNO_TRACKING",
    "MIUI_ANALYTICS", "MIUI_DAEMON", "GDPR_OPT_OUT", "GUARD_PROVIDER",
    "EXCESSIVE_WAKELOCK", "EXCESSIVE_FDS", "ZOMBIE_PROCESSES", "HIDDEN_GAP",
    "FRIDA_DETECTED", "XPOSED_DETECTED",
    NULL
};

static void test_every_entry_is_well_formed(void) {
    for (int i = 0; MAPPED_SIGNALS[i]; i++) {
        const mitre_entry_t *e = mitre_lookup(MAPPED_SIGNALS[i]);
        CHECK_MSG(e != NULL, "%s missing from mitre map", MAPPED_SIGNALS[i]);
        if (!e) continue;
        CHECK_MSG(e->technique_id && e->technique_id[0] == 'T' &&
                  strlen(e->technique_id) == 5,
                  "%s has malformed technique id \"%s\"",
                  MAPPED_SIGNALS[i], e->technique_id ? e->technique_id : "");
        CHECK_MSG(e->tactic && e->tactic[0], "%s missing tactic",
                  MAPPED_SIGNALS[i]);
        CHECK_MSG(e->technique_name && e->technique_name[0],
                  "%s missing technique name", MAPPED_SIGNALS[i]);
    }
}

int main(void) {
    RUN_TEST(test_lookup_known_signal);
    RUN_TEST(test_lookup_across_categories);
    RUN_TEST(test_unknown_and_null);
    RUN_TEST(test_returned_pointer_is_stable);
    RUN_TEST(test_every_entry_is_well_formed);
    return test_report();
}
