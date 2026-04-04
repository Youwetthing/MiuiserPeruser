#ifndef SYNDICATE_STATES_H
#define SYNDICATE_STATES_H

typedef enum {
    STATE_IDLE = 0,
    STATE_MONITORING,
    STATE_ACTION,
    STATE_RECOVERY,
    STATE_EMERGENCY
} syndicate_state_t;

typedef struct {
    syndicate_state_t current;
    syndicate_state_t previous;
    int ticks_in_state;
} brain_state_t;

#endif
