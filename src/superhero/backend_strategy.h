#ifndef BACKEND_STRATEGY_H
#define BACKEND_STRATEGY_H

typedef enum {
    BACKEND_NONE = 0,
    BACKEND_RISH,
    BACKEND_ADB,
    BACKEND_LOCAL
} BACKEND_TYPE;

typedef struct {
    int thermal;
    int battery;
    int cpu_freq;
} splinter_metrics_t;

BACKEND_TYPE  backend_strategy_select(void);
const char   *backend_name(BACKEND_TYPE b);

#endif
