#ifndef TURTLE_TELEMETRY_H
#define TURTLE_TELEMETRY_H

#include "backend_common.h"

void turtle_telemetry_init(const char *db_path);

void turtle_telemetry_log(
    const char *event_type,
    const char *cmd,
    backend_type_t expected,
    backend_type_t actual,
    int success,
    const char *metadata
);

#endif
