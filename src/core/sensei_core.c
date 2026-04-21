#include "sensei_core.h"
#include <stdbool.h>

/* Cortex interfaces */
bool cortex_protocol_parse(const char *raw, sensei_msg_t *out);
bool cortex_routing_route(const sensei_msg_t *msg, sensei_route_result_t *out);
void cortex_protocol_free_msg(sensei_msg_t *msg);
void cortex_routing_free_route(sensei_route_result_t *route);

bool sensei_cap_register(const char *worker, const char *command);

/* capability init */
static bool g_caps_inited = false;

static void sensei_core_ensure_caps(void) {
    if (g_caps_inited) return;
    g_caps_inited = true;

    sensei_cap_register("splinter",   "PING");
    sensei_cap_register("footrunner", "TEMP");
    sensei_cap_register("footrunner", "CPU");
    sensei_cap_register("superhero",  "SCAN");
    sensei_cap_register("splinter",   "WORKER");
}

/* API */
bool sensei_parse(const char *raw, sensei_msg_t *out_msg) {
    return cortex_protocol_parse(raw, out_msg);
}

bool sensei_route(const sensei_msg_t *msg, sensei_route_result_t *out_route) {
    sensei_core_ensure_caps();
    return cortex_routing_route(msg, out_route);
}

const char *sensei_status_string(sensei_status_t status) {
    switch (status) {
        case SENSEI_OK: return "OK";
        case SENSEI_ERR_PARSE: return "Parse error";
        case SENSEI_ERR_UNKNOWN_CMD: return "Unknown command";
        case SENSEI_ERR_NO_TARGET: return "No target worker";
        case SENSEI_ERR_INVALID: return "Invalid message";
        case SENSEI_ERR_INTERNAL: return "Internal error";
        default: return "Unknown status";
    }
}

void sensei_msg_free(sensei_msg_t *msg) {
    cortex_protocol_free_msg(msg);
}

void sensei_route_free(sensei_route_result_t *route) {
    cortex_routing_free_route(route);
}
