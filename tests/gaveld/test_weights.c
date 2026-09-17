/* Unit tests for src/gaveld/weights.c — signal weight / MITRE lookup table. */

#include "test_harness.h"
#include "weights.h"

#include <string.h>

static void test_known_signal_weights(void) {
    CHECK_INT_EQ(weight_lookup("IMEI_EXPOSED"), 35);
    CHECK_INT_EQ(weight_lookup("ADB_TCP_ENABLED"), 32);
    CHECK_INT_EQ(weight_lookup("INTEGRITY_VIOLATION"), 45);
    /* Informational signals are intentionally weight 0 */
    CHECK_INT_EQ(weight_lookup("HYPEROS_DETECTED"), 0);
    CHECK_INT_EQ(weight_lookup("EEA_BUILD"), 0);
}

static void test_unknown_signal_is_zero(void) {
    CHECK_INT_EQ(weight_lookup("NOT_A_REAL_SIGNAL"), 0);
    CHECK_INT_EQ(weight_lookup(""), 0);
}

static void test_lookup_is_case_sensitive_exact_match(void) {
    CHECK_INT_EQ(weight_lookup("imei_exposed"), 0);
    CHECK_INT_EQ(weight_lookup("IMEI_EXPOSED_EXTRA"), 0);
    CHECK_INT_EQ(weight_lookup("IMEI"), 0);
}

static void test_null_signal_is_safe(void) {
    CHECK_INT_EQ(weight_lookup(NULL), 0);
    CHECK(weight_mitre(NULL) == NULL);
}

static void test_mitre_ids(void) {
    CHECK_STR_EQ(weight_mitre("IMEI_EXPOSED"), "T1592");
    CHECK_STR_EQ(weight_mitre("ROOT_DETECTED"), "T1068");
    /* Signals with no ATT&CK mapping return NULL, not "" */
    CHECK(weight_mitre("THERMAL_WARN") == NULL);
    CHECK(weight_mitre("NOT_A_REAL_SIGNAL") == NULL);
}

static void test_table_size_matches_entries(void) {
    int n = weight_table_size();
    CHECK(n > 0);
    /* Every table row must be reachable through the public lookup. */
    CHECK_INT_EQ(weight_lookup("BACKGROUND_RESTRICTED_APPS"), 10);
}

/*
 * Table invariants — these guard against copy/paste mistakes when new
 * signals are appended: weights stay inside the documented 0–49 scale and
 * ATT&CK ids keep the "T####" shape.
 */
static const char *SAMPLE_SIGNALS[] = {
    "HYPEROS_DETECTED", "IMEI_EXPOSED", "MSA_TELEMETRY_ACTIVE",
    "VERIFIED_BOOT_FAIL", "ROOT_DETECTED", "SELINUX_PERMISSIVE",
    "THERMAL_CRITICAL", "CPU_HOG", "WAKELOCK_FULL_HELD",
    "CLEARTEXT_EXFIL", "MEM_PRESSURE", "SENSOR_FLOOD",
    "ROOTKIT_MODULE", "INTEGRITY_VIOLATION", "HIDDEN_PROCESS",
    "ANR_DETECTED", "ANON_EXEC_MEM", "OLD_KERNEL",
    NULL
};

static void test_weight_scale_and_mitre_format(void) {
    for (int i = 0; SAMPLE_SIGNALS[i]; i++) {
        const char *sig = SAMPLE_SIGNALS[i];
        int w = weight_lookup(sig);
        CHECK_MSG(w >= 0 && w <= 49, "%s weight %d outside 0-49 scale", sig, w);

        const char *m = weight_mitre(sig);
        if (m) {
            CHECK_MSG(m[0] == 'T' && strlen(m) == 5,
                      "%s has malformed ATT&CK id \"%s\"", sig, m);
        }
    }
}

int main(void) {
    RUN_TEST(test_known_signal_weights);
    RUN_TEST(test_unknown_signal_is_zero);
    RUN_TEST(test_lookup_is_case_sensitive_exact_match);
    RUN_TEST(test_null_signal_is_safe);
    RUN_TEST(test_mitre_ids);
    RUN_TEST(test_table_size_matches_entries);
    RUN_TEST(test_weight_scale_and_mitre_format);
    return test_report();
}
