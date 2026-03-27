#ifndef BACKEND_STRATEGY_H
#define BACKEND_STRATEGY_H

#include "sensei_types.h"
#include "backend_common.h"

/* Public alias for higher layers */
typedef backend_type_t BACKEND_TYPE;

/* Human-readable backend name */
const char *backend_name(BACKEND_TYPE t);

/* Select the best backend and return its type */
BACKEND_TYPE backend_strategy_select(void);

#endif /* BACKEND_STRATEGY_H */
