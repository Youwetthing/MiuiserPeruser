#include "../sensei_core.h"
#include <string.h>
#include <stdio.h>

#define MAX_CAPS 128

typedef struct {
    char worker[64];
    char command[64];
} sensei_cap_entry_t;

static sensei_cap_entry_t g_caps[MAX_CAPS];
static int g_cap_count = 0;

bool sensei_cap_register(const char *worker, const char *command) {
    if (!worker || !command) return false;

    /* Replace if already present */
    for (int i = 0; i < g_cap_count; i++) {
        if (strcmp(g_caps[i].worker, worker) == 0 &&
            strcmp(g_caps[i].command, command) == 0) {
            return true;
        }
    }

    if (g_cap_count >= MAX_CAPS) {
        fprintf(stderr, "[cap] registry full, cannot add %s -> %s\n",
                command, worker);
        return false;
    }

    strncpy(g_caps[g_cap_count].worker, worker,
            sizeof(g_caps[g_cap_count].worker) - 1);
    g_caps[g_cap_count].worker[sizeof(g_caps[g_cap_count].worker) - 1] = '\0';

    strncpy(g_caps[g_cap_count].command, command,
            sizeof(g_caps[g_cap_count].command) - 1);
    g_caps[g_cap_count].command[sizeof(g_caps[g_cap_count].command) - 1] = '\0';

    fprintf(stderr, "[cap] REGISTER %s -> %s\n", command, worker);

    g_cap_count++;
    return true;
}

bool sensei_cap_find_worker(const char *command, char *out_worker, size_t out_sz) {
    if (!command || !out_worker || out_sz == 0) return false;

    for (int i = 0; i < g_cap_count; i++) {
        if (strcmp(g_caps[i].command, command) == 0) {
            strncpy(out_worker, g_caps[i].worker, out_sz - 1);
            out_worker[out_sz - 1] = '\0';
            return true;
        }
    }

    return false;
}
