#ifndef BACKEND_MASK_H
#define BACKEND_MASK_H

#include "backend_common.h"

/*
 * Backend Mask Layer
 * Single source of truth for active backend identity
 */

typedef struct {
    backend_type_t active;
    const char *mask_name;
    int is_privileged;
} backend_mask_t;

const backend_mask_t *backend_get_mask(void);
int backend_mask_refresh(void);

#endif
