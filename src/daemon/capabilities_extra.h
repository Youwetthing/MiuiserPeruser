#pragma once

/*
 * capabilities_extra.h — daemon-facing capability state
 *
 * capabilities_t is the single struct for all daemon capability tracking.
 * All fields are int (not bool) — safe for IPC serialisation and
 * truth_engine consumption.
 *
 * Core capabilities.h / capabilities.c are for non-daemon use only.
 * The daemon links this file exclusively — no collision.
 */

typedef struct {
    int rish;
    int shizuku;
    int shizuku_script;
    int adb;
    int port_bridge_available;
    int port_bridge_info_ok;
} capabilities_t;

extern capabilities_t capabilities;

void detect_capabilities(void);
void print_capabilities_pretty(void);
void capability_print_summary(void);
void capability_print_hints(void);
