#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "splinter_selector.h"

/*
 * NOTE:
 * This file assumes you have some way to get DOCTOR-style diagnostics
 * into a string buffer, e.g.:
 *
 *   "sysfs_cpu=no sysfs_thermal=yes adb=yes thermalservice=yes backend=adb"
 *
 * Wire this to your real doctor backend. For now we declare an extern
 * and you can map it to whatever you already have.
 *
 * Expected contract:
 *   - returns 0 on success, non-zero on failure
 *   - writes a NUL-terminated status string into buf
 */
extern int doctor_get_status(char *buf, size_t buflen);

/* Simple helpers to search for "key=yes" in the doctor output */

static bool has_flag(const char *haystack, const char *needle_yes)
{
    if (!haystack || !needle_yes) {
        return false;
    }
    return strstr(haystack, needle_yes) != NULL;
}

static bool doctor_sysfs_thermal_ok(const char *s)
{
    return has_flag(s, "sysfs_thermal=yes");
}

static bool doctor_rish_ok(const char *s)
{
    return has_flag(s, "rish=yes");
}

static bool doctor_adb_ok(const char *s)
{
    return has_flag(s, "adb=yes");
}

static bool doctor_thermalservice_ok(const char *s)
{
    return has_flag(s, "thermalservice=yes");
}

static bool doctor_portbridge_ok(const char *s)
{
    return has_flag(s, "portbridge=yes");
}

/*
 * Splinter's first-pass decision tree:
 *
 * Priority:
 *   1. SYSFS        (fast, local, no IPC)
 *   2. RISH         (lightweight, no adb server)
 *   3. ADB          (universal but heavy)
 *   4. PORTBRIDGE   (MIUI/Xiaomi bypass)
 *   5. NONE         (no usable backend)
 *
 * This uses only DOCTOR output for now. Later we can extend this to
 * consult FULL_SCAN results or cached capability state.
 */

backend_kind_t splinter_pick_backend(void)
{
    char status[512];

    if (doctor_get_status(status, sizeof(status)) != 0) {
        /* If we can't even talk to Doctor, we can't make a safe choice */
        return BACKEND_NONE;
    }

    /* 1. Prefer sysfs thermal if available */
    if (doctor_sysfs_thermal_ok(status)) {
        return BACKEND_SYSFS;
    }

    /* 2. Then rish, once you have rish detection wired into Doctor */
    if (doctor_rish_ok(status)) {
        return BACKEND_RISH;
    }

    /* 3. Then adb, but only if thermalservice is also available */
    if (doctor_adb_ok(status) && doctor_thermalservice_ok(status)) {
        return BACKEND_ADB;
    }

    /* 4. Then portbridge as a MIUI/Xiaomi bypass path */
    if (doctor_portbridge_ok(status)) {
        return BACKEND_PORTBRIDGE;
    }

    /* 5. Nothing usable */
    return BACKEND_NONE;
}

/* Human-readable names for logs, doctor summaries, etc. */

const char *splinter_backend_name(backend_kind_t b)
{
    switch (b) {
    case BACKEND_SYSFS:
        return "sysfs";
    case BACKEND_RISH:
        return "rish";
    case BACKEND_ADB:
        return "adb";
    case BACKEND_PORTBRIDGE:
        return "portbridge";
    case BACKEND_NONE:
    default:
        return "none";
    }
}
