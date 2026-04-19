#include "compat/sensei_compat.h"
#include "syndicate_db.h"
#include <stdio.h>

void fugitoid_log_event(const char* event) {
    // Fugitoid uses the April Shim symbols to record cross-daemon trends
    syndicate_db_log("FUGITOID", "ANALYSIS", event);
}
