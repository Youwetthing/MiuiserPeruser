#define _GNU_SOURCE
#include "enforce.h"
#include "config.h"
#include "log.h"
#include "tier.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ── Backend selection ───────────────────────────────────────────────────── */

typedef enum { BACKEND_RISH, BACKEND_ADB, BACKEND_DIRECT } backend_t;
static backend_t g_backend = BACKEND_DIRECT;

static int probe_rish(void) {
    return access(RISH_PATH, X_OK) == 0;
}

static int probe_adb(void) {
    return system("adb devices 2>/dev/null | grep -q 'device$'") == 0;
}

void enforce_init(void) {
    if (probe_rish()) {
        g_backend = BACKEND_RISH;
        glog("INFO", "enforce backend: rish (%s)", RISH_PATH);
    } else if (probe_adb()) {
        g_backend = BACKEND_ADB;
        glog("INFO", "enforce backend: adb");
    } else {
        g_backend = BACKEND_DIRECT;
        glog("INFO", "enforce backend: direct popen (limited privilege)");
    }
}

/* ── Command runner ──────────────────────────────────────────────────────── */

static int run_cmd(const char *shell_cmd) {
    char full[1024];

    switch (g_backend) {
        case BACKEND_RISH:
            snprintf(full, sizeof(full), "%s -c '%s' 2>/dev/null",
                     RISH_PATH, shell_cmd);
            break;
        case BACKEND_ADB:
            snprintf(full, sizeof(full), "adb shell '%s' 2>/dev/null",
                     shell_cmd);
            break;
        default:
            snprintf(full, sizeof(full), "%s 2>/dev/null", shell_cmd);
            break;
    }

    glog("DEBUG", "enforce_run: %s", full);
    int rc = system(full);
    if (rc != 0)
        glog("WARN", "enforce_run rc=%d cmd=%s", rc, shell_cmd);
    return rc;
}

/* ── Action mapping ──────────────────────────────────────────────────────── */

/*
 * Returns 1 if pm disable-user is permitted for this source.
 * Sovereignty and system apps are always excluded.
 */
static int can_disable_user(const char *source) {
    if (tier_is_sovereignty(source)) return 0;
    if (tier_is_system(source))      return 0;
    if (tier_is_own_daemon(source))  return 0;
    return 1;
}

/* ── Main enforce entry ──────────────────────────────────────────────────── */

int enforce_execute(const char *source, const char *verdict,
                    const char *case_id, double score) {
    char cmd[512];
    glog("INFO", "ENFORCE src=%s verdict=%s score=%.2f case=%s",
         source, verdict, score, case_id);

    if (strcmp(verdict, "WARNED") == 0) {
        /* Soft — am kill only. Works for foreground processes. */
        snprintf(cmd, sizeof(cmd), "am kill %s", source);
        run_cmd(cmd);

    } else if (strcmp(verdict, "QUARANTINED") == 0 ||
               strcmp(verdict, "HOUSE_ARREST") == 0) {
        /* Hard stop — tier-gated */
        snprintf(cmd, sizeof(cmd), "am force-stop %s", source);
        run_cmd(cmd);

    } else if (strcmp(verdict, "JAILED") == 0) {
        /* Force stop always */
        snprintf(cmd, sizeof(cmd), "am force-stop %s", source);
        run_cmd(cmd);

        /* pm disable-user only for non-system, non-sovereignty apps */
        if (can_disable_user(source)) {
            snprintf(cmd, sizeof(cmd),
                     "pm disable-user --user 0 %s", source);
            run_cmd(cmd);
            glog("INFO", "ENFORCE pm_disable src=%s", source);
        } else {
            glog("INFO",
                 "ENFORCE pm_disable skipped — tier policy src=%s", source);
        }

    } else {
        glog("WARN", "ENFORCE unknown verdict=%s src=%s — no action",
             verdict, source);
        return -1;
    }

    return 0;
}
