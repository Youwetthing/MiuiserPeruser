#ifndef BACKEND_DRIFT_H
#define BACKEND_DRIFT_H

#include "backend_common.h"

typedef struct {
    int mismatch_count;
    backend_type_t last_expected;
    backend_type_t last_actual;
} backend_drift_t;

void drift_record(backend_type_t expected, backend_type_t actual);
int drift_is_unstable(void);
void drift_reset(void);

#endif
