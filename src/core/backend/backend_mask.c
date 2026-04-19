#include "backend_mask.h"
#include "backend_strategy.h"
#include <string.h>

static backend_mask_t g_mask;

const backend_mask_t *backend_get_mask(void)
{
    return &g_mask;
}

int backend_mask_refresh(void)
{
    BACKEND_TYPE t = backend_strategy_select();

    g_mask.active = t;

    switch (t) {
        case BACKEND_SHIZUKU:
            g_mask.mask_name = "shizuku_mask";
            g_mask.is_privileged = 1;
            break;

        case BACKEND_RISH:
            g_mask.mask_name = "rish_mask";
            g_mask.is_privileged = 1;
            break;

        case BACKEND_ADB:
            g_mask.mask_name = "adb_mask";
            g_mask.is_privileged = 0;
            break;

        default:
            g_mask.mask_name = "fallback_mask";
            g_mask.is_privileged = 0;
            break;
    }

    return 0;
}
