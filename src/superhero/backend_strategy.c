/*
 * MiuiserPeruser – Backend Strategy
 * Splinter's logic for choosing sysport, rish, shizuku, or adb.
 */

#include <stdio.h>
#include "backend_strategy.h"
#include "april_platform.h"
#include "april_event.h"

extern void april_log(const char* level, const char* format, ...);

/* ---- Backend probes -------------------------------------------------- */

static int probe_sysport(void) {
    /* TODO: real sysport probe */
    return 70; /* placeholder score */
}

static int probe_rish(void) {
    /* TODO: real rish probe */
    return 60; /* placeholder score */
}

static int probe_shizuku(void) {
    /* TODO: real shizuku probe */
    return 50; /* placeholder score */
}

static int probe_adb(void) {
    /* TODO: real adb probe */
    return 30; /* placeholder score */
}

/* ---- Name helper ----------------------------------------------------- */

const char* backend_name(BACKEND_TYPE type) {
    switch (type) {
        case BACKEND_SYSPORT: return "sysport";
        case BACKEND_RISH:    return "rish";
        case BACKEND_SHIZUKU: return "shizuku";
        case BACKEND_ADB:     return "adb";
        default:              return "none";
    }
}

/* ---- Selection logic ------------------------------------------------- */

BACKEND_SELECTION backend_select_best(void) {
    BACKEND_SELECTION sel = { BACKEND_NONE, 0 };

    int sysport_score = probe_sysport();
    int rish_score    = probe_rish();
    int shizuku_score = probe_shizuku();
    int adb_score     = probe_adb();

    april_log("INFO", "Backend scores: sysport=%d, rish=%d, shizuku=%d, adb=%d",
              sysport_score, rish_score, shizuku_score, adb_score);

    /* Pick the highest score */
    if (sysport_score > sel.score) {
        sel.type = BACKEND_SYSPORT;
        sel.score = sysport_score;
    }
    if (rish_score > sel.score) {
        sel.type = BACKEND_RISH;
        sel.score = rish_score;
    }
    if (shizuku_score > sel.score) {
        sel.type = BACKEND_SHIZUKU;
        sel.score = shizuku_score;
    }
    if (adb_score > sel.score) {
        sel.type = BACKEND_ADB;
        sel.score = adb_score;
    }

    april_log("INFO", "Selected backend: %s (score=%d)",
              backend_name(sel.type), sel.score);

    return sel;
}
