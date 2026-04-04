#include <stdio.h>
#include "syndicate_states.h"
#include "syndicate_health.h"

extern health_monitor_t shadow_health;
brain_state_t syndicate_brain = {STATE_MONITORING};

void process_state_transition() {
    // If the Muscle is Sick or Offline, move to Recovery
    if (shadow_health.state == STATUS_SICK || shadow_health.state == STATUS_OFFLINE) {
        syndicate_brain.current = STATE_RECOVERY;
    } else {
        syndicate_brain.current = STATE_MONITORING;
    }
}
