#include "backends/backend_common.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/* The original rish-based identity-hijack DEX (route #1, route #2). */
#define PATCHED_DEX_PATH "/data/data/com.termux/files/home/MiuiserPeruser/rish_shizuku.dex"

/* The third route: our custom DEX that binds directly to Shizuku, no rish. */
#define DIRECT_DEX_PATH  "/data/data/com.termux/files/home/MiuiserPeruser/rish_shizuku_direct.dex"

/* The UDS the direct-route helper listens on (kept in lockstep with the
 * SHZ_DIRECT_SOCKET_PATH constant in backend_shizuku_direct.c). */
#define DIRECT_SOCKET_PATH \
    "/data/data/com.termux/files/usr/tmp/miuiser-shizuku.sock"

/* Allow an operator override: "RISH" forces route #1/#2, "DIRECT" forces #3. */
#define BACKEND_FORCE_ENV "MIUISER_BACKEND"

static backend_type_t g_active_backend = BACKEND_NONE;

/*
 * BACKEND_SELECT_BEST
 *
 * Preference order:
 *   1. $MIUISER_BACKEND override (DIRECT|RISH|ADB)
 *   2. SHIZUKU_DIRECT  -- our custom DEX + helper UDS (or just the DEX; we'll spawn)
 *   3. RISH            -- stock rish + patched Shizuku DEX
 *   4. ADB             -- universal fallback
 */
int backend_select_best(void) {
    printf("[Selector] Scanning for Power Sources...\n");

    /* 1. Explicit override */
    const char *force = getenv(BACKEND_FORCE_ENV);
    if (force && *force) {
        if (strcasecmp(force, "DIRECT") == 0 || strcasecmp(force, "SHIZUKU_DIRECT") == 0) {
            printf("[Selector] OVERRIDE: forced SHIZUKU_DIRECT via $%s\n", BACKEND_FORCE_ENV);
            g_active_backend = BACKEND_SHIZUKU_DIRECT;
            return 0;
        }
        if (strcasecmp(force, "RISH") == 0) {
            printf("[Selector] OVERRIDE: forced RISH via $%s\n", BACKEND_FORCE_ENV);
            g_active_backend = BACKEND_RISH;
            return 0;
        }
        if (strcasecmp(force, "ADB") == 0) {
            printf("[Selector] OVERRIDE: forced ADB via $%s\n", BACKEND_FORCE_ENV);
            g_active_backend = BACKEND_ADB;
            return 0;
        }
        printf("[Selector] WARN: unknown $%s='%s', ignoring\n", BACKEND_FORCE_ENV, force);
    }

    /* 2. The direct route is preferred whenever its dex is present.
     *    Live socket is a bonus (we attach instantly); dex-only means the
     *    backend's init() will spawn the helper itself. */
    bool have_direct_dex  = (access(DIRECT_DEX_PATH, F_OK) == 0);
    bool have_direct_sock = (access(DIRECT_SOCKET_PATH, F_OK) == 0);
    if (have_direct_dex) {
        printf("[Selector] DETECTED: Direct-route DEX at %s%s\n",
               DIRECT_DEX_PATH, have_direct_sock ? " (helper already up)" : "");
        printf("[Selector] STATUS: SHIZUKU_DIRECT armed -- rish bypassed.\n");
        g_active_backend = BACKEND_SHIZUKU_DIRECT;
        return 0;
    }

    /* 3. The legacy rish-based hijack */
    if (access(PATCHED_DEX_PATH, F_OK) == 0) {
        printf("[Selector] DETECTED: Patched Shizuku DEX at %s\n", PATCHED_DEX_PATH);
        printf("[Selector] STATUS: Identity 'com.termux' wearing Shizuku Mask (via rish).\n");
        g_active_backend = BACKEND_RISH;
        return 0;
    }

    /* 4. Universal fallback */
    printf("[Selector] WARN: no privileged DEX found. Falling back to ADB.\n");
    g_active_backend = BACKEND_ADB;
    return 0;
}

const char *backend_get_current_name(void) {
    switch (g_active_backend) {
        case BACKEND_SHIZUKU_DIRECT: return "SHIZUKU_DIRECT (Privileged/No-rish)";
        case BACKEND_RISH:           return "RISH (Privileged/Patched)";
        case BACKEND_ADB:            return "ADB (Universal)";
        default:                     return "NONE";
    }
}

/* Connects the logic to the actual execution vtables */
const backend_vtable_t *backend_get_active_vtable(void) {
    extern const backend_vtable_t backend_rish_vtable;
    extern const backend_vtable_t backend_adb_vtable;
    extern const backend_vtable_t backend_shizuku_direct_vtable;

    if (g_active_backend == BACKEND_SHIZUKU_DIRECT) return &backend_shizuku_direct_vtable;
    if (g_active_backend == BACKEND_RISH)           return &backend_rish_vtable;
    if (g_active_backend == BACKEND_ADB)            return &backend_adb_vtable;

    return NULL;
}
