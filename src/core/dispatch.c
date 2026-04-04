#include "../sensei_core.h"
#include <string.h>
#include <stdio.h>

/*
 * Dispatch Cortex (Refined)
 *
 * Produces a dispatch plan for Splinterd to execute.
 * No sockets. No I/O. Pure cognition.
 */

bool sensei_dispatch_plan(const sensei_msg_t *msg,
                          const sensei_route_result_t *route,
                          sensei_dispatch_plan_t *out)
{
    if (!msg || !route || !out) return false;

    if (route->worker_name[0] == '\0') {
        fprintf(stderr, "[dispatch-plan] No worker in route\n");
        return false;
    }

    /* Fill worker */
    strncpy(out->worker, route->worker_name, sizeof(out->worker) - 1);
    out->worker[sizeof(out->worker) - 1] = '\0';

    /* Fill payload */
    strncpy(out->payload, msg->raw, sizeof(out->payload) - 1);
    out->payload[sizeof(out->payload) - 1] = '\0';

    fprintf(stderr, "[dispatch-plan] PLAN → worker=\"%s\" payload=\"%s\"\n",
            out->worker, out->payload);

    return true;
}
