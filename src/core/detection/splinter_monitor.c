#include "backend_mask.h"

int splinter_status_check(void)
{
    const backend_mask_t *mask = backend_get_mask();

    if (!mask)
        return 0;

    return (mask->active != BACKEND_NONE);
}
