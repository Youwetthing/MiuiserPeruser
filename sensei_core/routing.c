#include "../sensei_core.h"
#include <string.h>
#include <strings.h>
#include <stdbool.h>

/* From capabilities.c */
bool sensei_cap_find_worker(const char *command, char *out_worker, size_t out_sz);

/* Simple static mapping from worker name → fd placeholder */
static int worker_fd(const char *worker) {
    if (strcasecmp(worker, "splinter") == 0)   return 1;
    if (strcasecmp(worker, "footrunner") == 0) return 2;
    if (strcasecmp(worker, "superhero") == 0)  return 3;
    return -1;
}

bool cortex_routing_route(const sensei_msg_t *msg, sensei_route_result_t *out) {
    if (!msg || !out) return false;

    char worker[64];
    if (!sensei_cap_find_worker(msg->command, worker, sizeof(worker))) {
        out->target_fd = -1;
        out->worker_name[0] = '\0';
        return false;
    }

    int fd = worker_fd(worker);
    if (fd < 0) {
        out->target_fd = -1;
        out->worker_name[0] = '\0';
        return false;
    }

    out->target_fd = fd;
    strncpy(out->worker_name, worker, sizeof(out->worker_name) - 1);
    out->worker_name[sizeof(out->worker_name) - 1] = '\0';
    return true;
}

void cortex_routing_free_route(sensei_route_result_t *route) {
    (void)route;
}
