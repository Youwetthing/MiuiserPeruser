#ifndef EXEC_BRIDGE_H
#define EXEC_BRIDGE_H

#include "backend_common.h"

typedef struct {
    char *output;
    backend_type_t backend_used;
    int success;
} exec_result_t;

/* unified execution entry */
exec_result_t exec_bridge_run(const char *cmd, backend_type_t preferred);

#endif
