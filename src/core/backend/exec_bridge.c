#include "exec_bridge.h"
#include "backend_mask.h"
#include "turtlepower.h"
#include "backend_drift.h"

#include "backend_adb.h"
#include "backend_rish.h"
#include "backend_shizuku.h"
#include "backend_termux_fallback.h"

#include <stdlib.h>
#include <string.h>

static char *run_backend(const char *cmd, backend_type_t t)
{
    switch (t) {
        case BACKEND_ADB:
            return backend_adb_exec(cmd);

        case BACKEND_RISH:
            return rish_pipe_command(cmd);

        case BACKEND_SHIZUKU:
            return rish_pipe_command(cmd);

        default:
            return strdup("NO_BACKEND");
    }
}

exec_result_t exec_bridge_run(const char *cmd, backend_type_t preferred)
{
    exec_result_t res = {0};

    turtle_decision_t policy = turtlepower_gate(cmd, preferred);

    if (!policy.allow_exec) {
        res.output = strdup(policy.reason);
        res.backend_used = preferred;
        res.success = 0;

        drift_record(preferred, BACKEND_NONE);
        return res;
    }

    res.backend_used = preferred;
    res.output = run_backend(cmd, preferred);

    res.success =
        (res.output != NULL &&
         strcmp(res.output, "NO_BACKEND") != 0);

    backend_mask_apply_exec(res.backend_used, res.success);

    /* 🧠 DRIFT TRACKING (core addition) */
    const backend_mask_t *mask = backend_get_mask();
    drift_record(preferred, mask->active);

    return res;
}
