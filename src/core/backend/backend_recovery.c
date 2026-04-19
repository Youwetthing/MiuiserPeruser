#include "backend_recovery.h"
#include "backend_mask.h"
#include "backend_strategy.h"
#include "backend_drift.h"

#include <stdio.h>

/* safest fallback choice */
static backend_type_t safe_fallback(void)
{
    return BACKEND_TERMUX_FALLBACK;
}

/* full system reset of backend state */
void backend_recovery_reset_state(void)
{
    drift_reset();
    backend_mask_refresh();
}

/* main recovery trigger */
int backend_recovery_trigger(void)
{
    backend_type_t recovered = backend_strategy_select();

    /* if strategy is still unstable, force safe mode */
    if (recovered == BACKEND_NONE)
        recovered = safe_fallback();

    backend_mask_refresh();
    drift_reset();

    return (int)recovered;
}
