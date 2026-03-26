#ifndef BACKEND_STRATEGY_H
#define BACKEND_STRATEGY_H

#include <stdint.h>
#include "sensei_types.h"

/* Available backend types */
typedef enum {
    BACKEND_NONE = 0,
    BACKEND_SYSPORT,
    BACKEND_RISH,
    BACKEND_SHIZUKU,
    BACKEND_ADB
} BACKEND_TYPE;

/* Result of backend selection */
typedef struct {
    BACKEND_TYPE type;
    int score;
} BACKEND_SELECTION;

/* Detect and score all backends, return the best one. */
BACKEND_SELECTION backend_select_best(void);

/* Convert backend enum to string */
const char* backend_name(BACKEND_TYPE type);

#endif /* BACKEND_STRATEGY_H */
