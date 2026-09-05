/* Unit tests for src/gaveld/tier.c — source trust tiers. */

#include "test_harness.h"
#include "gaveld_test_env.h"
#include "tier.h"

#include <unistd.h>

static void test_own_daemon_detection(void) {
    CHECK_INT_EQ(tier_is_own_daemon("rahzerd"), 1);
    CHECK_INT_EQ(tier_is_own_daemon("gaveld"), 1);
    CHECK_INT_EQ(tier_is_own_daemon("court_orchestrator"), 1);
    CHECK_INT_EQ(tier_is_own_daemon("com.spotify.music"), 0);
    CHECK_INT_EQ(tier_is_own_daemon(""), 0);
}

static void test_own_daemon_uses_basename(void) {
    CHECK_INT_EQ(tier_is_own_daemon("/data/data/com.termux/files/home/bin/rahzerd"), 1);
    CHECK_INT_EQ(tier_is_own_daemon("/usr/bin/shredderd"), 1);
    /* Trailing slash leaves an empty basename — must not match. */
    CHECK_INT_EQ(tier_is_own_daemon("/usr/bin/rahzerd/"), 0);
    /* Substrings of a daemon name are not daemons. */
    CHECK_INT_EQ(tier_is_own_daemon("rahzerd_helper"), 0);
}

static void test_system_prefixes(void) {
    CHECK_INT_EQ(tier_is_system("com.miui.securitycenter"), 1);
    CHECK_INT_EQ(tier_is_system("com.xiaomi.finddevice"), 1);
    CHECK_INT_EQ(tier_is_system("android.process.media"), 1);
    CHECK_INT_EQ(tier_is_system("org.codeaurora.ims"), 1);
    /* Prefix must be at position 0. */
    CHECK_INT_EQ(tier_is_system("net.com.miui.fake"), 0);
    CHECK_INT_EQ(tier_is_system("com.whatsapp"), 0);
    CHECK_INT_EQ(tier_is_system(""), 0);
}

static void test_sovereignty_list(void) {
    gt_write_file(SOVEREIGNTY_LIST,
                  "# user-trusted apps\n"
                  "\n"
                  "com.whatsapp|messaging\n"
                  "org.thoughtcrime.securesms   \n"
                  "com.brave.browser\n");

    CHECK_INT_EQ(tier_is_sovereignty("com.whatsapp"), 1);
    /* Trailing whitespace in the list is trimmed. */
    CHECK_INT_EQ(tier_is_sovereignty("org.thoughtcrime.securesms"), 1);
    CHECK_INT_EQ(tier_is_sovereignty("com.brave.browser"), 1);
    /* Comments and blank lines are not entries. */
    CHECK_INT_EQ(tier_is_sovereignty("# user-trusted apps"), 0);
    CHECK_INT_EQ(tier_is_sovereignty("com.facebook.katana"), 0);
    /* Notes after the pipe are not part of the package name. */
    CHECK_INT_EQ(tier_is_sovereignty("com.whatsapp|messaging"), 0);

    unlink(SOVEREIGNTY_LIST);
    /* Missing list means nothing is sovereign, not an error. */
    CHECK_INT_EQ(tier_is_sovereignty("com.whatsapp"), 0);
}

static void test_modifier_precedence(void) {
    gt_write_file(SOVEREIGNTY_LIST, "com.miui.player\ncom.whatsapp\n");

    /* Own daemon wins over everything else. */
    CHECK_DBL_EQ(tier_modifier("rahzerd"), TIER_MOD_OWN_DAEMON);
    /* Sovereignty wins over the system prefix. */
    CHECK_DBL_EQ(tier_modifier("com.miui.player"), TIER_MOD_SOVEREIGNTY);
    CHECK_DBL_EQ(tier_modifier("com.whatsapp"), TIER_MOD_SOVEREIGNTY);
    /* System package that is not sovereign. */
    CHECK_DBL_EQ(tier_modifier("com.miui.securitycenter"), TIER_MOD_MIUI_AOSP);
    /* Everything else is untrusted. */
    CHECK_DBL_EQ(tier_modifier("com.facebook.katana"), TIER_MOD_UNKNOWN);

    unlink(SOVEREIGNTY_LIST);
}

static void test_modifier_null_and_empty(void) {
    CHECK_DBL_EQ(tier_modifier(NULL), TIER_MOD_UNKNOWN);
    CHECK_DBL_EQ(tier_modifier(""), TIER_MOD_UNKNOWN);
}

int main(void) {
    gt_env_init();
    RUN_TEST(test_own_daemon_detection);
    RUN_TEST(test_own_daemon_uses_basename);
    RUN_TEST(test_system_prefixes);
    RUN_TEST(test_sovereignty_list);
    RUN_TEST(test_modifier_precedence);
    RUN_TEST(test_modifier_null_and_empty);
    return test_report();
}
