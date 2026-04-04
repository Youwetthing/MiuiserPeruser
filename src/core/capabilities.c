#include "../sensei_core.h"
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

typedef struct {
    char worker[64];
    char command[64];
} cap_entry_t;

static cap_entry_t CAP_TABLE[64];
static int CAP_COUNT = 0;

bool sensei_cap_register(const char *worker, const char *command) {
    if (!worker || !command) return false;
    if (CAP_COUNT >= (int)(sizeof(CAP_TABLE) / sizeof(CAP_TABLE[0]))) {
        fprintf(stderr, "[cap] table full, cannot register %s:%s\n", worker, command);
        return false;
    }

    for (int i = 0; i < CAP_COUNT; i++) {
        if (strcmp(CAP_TABLE[i].worker, worker) == 0 &&
            strcmp(CAP_TABLE[i].command, command) == 0) {
            return true;
        }
    }

    strncpy(CAP_TABLE[CAP_COUNT].worker, worker, sizeof(CAP_TABLE[CAP_COUNT].worker) - 1);
    strncpy(CAP_TABLE[CAP_COUNT].command, command, sizeof(CAP_TABLE[CAP_COUNT].command) - 1);
    CAP_COUNT++;
    return true;
}

bool sensei_cap_find_worker(const char *command, char *out_worker, size_t out_sz) {
    if (!command || !out_worker || out_sz == 0) return false;

    for (int i = 0; i < CAP_COUNT; i++) {
        if (strcmp(CAP_TABLE[i].command, command) == 0) {
            strncpy(out_worker, CAP_TABLE[i].worker, out_sz - 1);
            out_worker[out_sz - 1] = '\0';
            return true;
        }
    }

    return false;
}
