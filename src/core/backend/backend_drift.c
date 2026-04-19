#include "backend_drift.h"

static backend_drift_t g_drift = {0};

void drift_record(backend_type_t expected, backend_type_t actual)
{
    if (expected != actual) {
        g_drift.mismatch_count++;
    } else {
        /* small decay so system can recover */
        if (g_drift.mismatch_count > 0)
            g_drift.mismatch_count--;
    }

    g_drift.last_expected = expected;
    g_drift.last_actual = actual;
}

int drift_is_unstable(void)
{
    /* threshold = system starts becoming unreliable */
    return g_drift.mismatch_count >= 3;
}

void drift_reset(void)
{
    g_drift.mismatch_count = 0;
    g_drift.last_expected = BACKEND_NONE;
    g_drift.last_actual = BACKEND_NONE;
}
