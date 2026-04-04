#include "backends/backend_common.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/* The Identity of the Genius Patch */
#define PATCHED_DEX_PATH "/data/data/com.termux/files/home/MiuiserPeruser/rish_shizuku.dex"

static backend_type_t g_active_backend = BACKEND_NONE;

/* 
 * BACKEND_SELECT_BEST
 * This is the heart of the Nervous System. 
 * It forces the system to use your hijacked Shizuku identity.
 */
int backend_select_best(void) {
    printf("[Selector] Scanning for Power Sources...\n");

    // 1. Check for the "Genius" Hijack
    if (access(PATCHED_DEX_PATH, F_OK) == 0) {
        printf("[Selector] DETECTED: Patched Shizuku DEX at %s\n", PATCHED_DEX_PATH);
        printf("[Selector] STATUS: Identity 'com.termux' wearing Shizuku Mask.\n");
        g_active_backend = BACKEND_RISH;
        return 0;
    }

    // 2. Fallback to ADB if the DEX is missing
    printf("[Selector] WARN: Patched DEX not found. Falling back to ADB.\n");
    g_active_backend = BACKEND_ADB;
    return 0;
}

const char *backend_get_current_name(void) {
    switch (g_active_backend) {
        case BACKEND_RISH: return "RISH (Privileged/Patched)";
        case BACKEND_ADB:  return "ADB (Universal)";
        default:           return "NONE";
    }
}

/* Connects the logic to the actual execution vtables */
const backend_vtable_t *backend_get_active_vtable(void) {
    extern const backend_vtable_t backend_rish_vtable;
    extern const backend_vtable_t backend_adb_vtable;

    if (g_active_backend == BACKEND_RISH) return &backend_rish_vtable;
    if (g_active_backend == BACKEND_ADB)  return &backend_adb_vtable;
    
    return NULL;
}
