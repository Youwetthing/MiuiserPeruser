#include "splinter_selector.h"
#include <string.h>

/* * TIGHTENED SELECTOR: No Doctor needed.
 * We hardcode the priority: 
 * 1. SYSFS (Fastest)
 * 2. ADB (Universal)
 * 3. PORTBRIDGE (Xiaomi/MIUI)
 */

backend_kind_t splinter_pick_backend(void) {
    // For now, we assume the Sewer is healthy. 
    // You can manually toggle these to test different pathways.
    return BACKEND_ADB; 
}

const char *splinter_backend_name(backend_kind_t b) {
    switch (b) {
        case BACKEND_SYSFS:      return "sysfs";
        case BACKEND_RISH:       return "rish";
        case BACKEND_ADB:        return "adb";
        case BACKEND_PORTBRIDGE: return "portbridge";
        default:                 return "none";
    }
}
