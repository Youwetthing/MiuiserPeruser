#ifndef SENSEI_CORE_H
#define SENSEI_CORE_H

#include <stdbool.h>
#include <stddef.h>

/* ------------------------------
 *  Message Structure
 * ------------------------------ */
typedef struct {
    char raw[1024];
    char command[64];
    char args[16][128];
    int  argc;
} sensei_msg_t;

/* ------------------------------
 *  Routing Result
 * ------------------------------ */
typedef struct {
    char worker_name[64];
    int  target_fd;   /* legacy, unused in refined dispatch */
} sensei_route_result_t;

/* ------------------------------
 *  Dispatch Plan (Refined)
 * ------------------------------ */
typedef struct {
    char worker[64];
    char payload[1024];
} sensei_dispatch_plan_t;

/* ------------------------------
 *  Public API
 * ------------------------------ */

/* Protocol Cortex */
bool sensei_parse(const char *raw, sensei_msg_t *out_msg);
void sensei_msg_free(sensei_msg_t *msg);

/* Routing Cortex */
bool sensei_route(const sensei_msg_t *msg, sensei_route_result_t *out_route);
void sensei_route_free(sensei_route_result_t *route);

/* Dispatch Cortex (Refined) */
bool sensei_dispatch_plan(
    const sensei_msg_t *msg,
    const sensei_route_result_t *route,
    sensei_dispatch_plan_t *out
);

/* Capability Cortex */
bool sensei_cap_register(const char *worker, const char *command);
bool sensei_cap_find_worker(const char *command, char *out_worker, size_t out_sz);

/* Status enum (optional) */
typedef enum {
    SENSEI_OK = 0,
    SENSEI_ERR_PARSE,
    SENSEI_ERR_UNKNOWN_CMD,
    SENSEI_ERR_NO_TARGET,
    SENSEI_ERR_INVALID,
    SENSEI_ERR_INTERNAL
} sensei_status_t;

const char *sensei_status_string(sensei_status_t status);

#endif /* SENSEI_CORE_H */
