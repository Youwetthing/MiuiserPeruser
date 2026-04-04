// splinter_footwire.c — Splinter wiring to Krang + Fugitoid
// Splinter receives FOOT RESULT lines from Footrunner and forwards them.

#include <stdio.h>
#include <string.h>

// External functions provided by Krang + Fugitoid
void krang_handle_foot_result(const char *line);
void fugitoid_store_foot_result(const char *line);

// ------------------------------------------------------------
// Splinter wiring: handle incoming Foot results
// ------------------------------------------------------------
void splinter_forward_foot_result(const char *line) {
    if (!line) return;

    // Forward to Krang for interpretation
    krang_handle_foot_result(line);

    // Forward to Fugitoid for storage
    fugitoid_store_foot_result(line);
}
