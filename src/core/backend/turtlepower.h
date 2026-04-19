#ifndef TURTLEPOWER_H
#define TURTLEPOWER_H

#include "backend_common.h"

typedef struct {
    int allow_exec;
    int allow_privilege;
    const char *reason;
} turtle_decision_t;

/* core policy gate */
turtle_decision_t turtlepower_gate(const char *cmd, backend_type_t backend);

#endif
