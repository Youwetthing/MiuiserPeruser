#ifndef SYNDICATE_HEALTH_H
#define SYNDICATE_HEALTH_H

typedef enum {
    STATUS_HEALTHY,
    STATUS_SICK,
    STATUS_OFFLINE,
    STATUS_DEAD
} health_status_t;

typedef struct {
    health_status_t state;
    int missed_pings;
} health_monitor_t;

#endif /* SYNDICATE_HEALTH_H */
