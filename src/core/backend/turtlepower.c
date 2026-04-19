#include "turtlepower.h"
#include "backend_mask.h"
#include "backend_drift.h"
#include "backend_recovery.h"
#include <string.h>

static int is_privileged_backend(backend_type_t t)
{
    return (t == BACKEND_SHIZUKU || t == BACKEND_RISH);
}

turtle_decision_t turtlepower_gate(const char *cmd, backend_type_t backend)
{
    turtle_decision_t d = {0};

    const backend_mask_t *mask = backend_get_mask();

    /* 🧠 HARD SAFETY: drift recovery trigger */
    if (drift_is_unstable()) {
        backend_recovery_trigger();

        d.allow_exec = 0;
        d.reason = "recovery triggered due to backend drift";
        return d;
    }

    if (cmd && strstr(cmd, "rm -rf")) {
        d.allow_exec = 0;
        d.reason = "dangerous command blocked";
        return d;
    }

    if (strstr(cmd, "dumpsys") || strstr(cmd, "setprop")) {
        if (!is_privileged_backend(mask->active)) {
            d.allow_exec = 0;
            d.reason = "privileged command requires elevated backend";
            return d;
        }
        d.allow_privilege = 1;
    }

    if (backend != mask->active) {
        d.allow_exec = 1;
        d.reason = "backend mismatch detected";
        return d;
    }

    d.allow_exec = 1;
    d.allow_privilege = is_privileged_backend(backend);
    d.reason = "ok";

    return d;
}
