#include <stdlib.h>
#include <string.h>
#include "backend_adb.h"

// Returns 1 if ADB is ready, 0 otherwise
int adb_activate(void)
{
    // Force ADB server to start
    char *devs = backend_adb_exec("devices");
    if (!devs) return 0;
    free(devs);

    // Check device state
    char *state = backend_adb_exec("get-state");
    if (!state) return 0;

    int ok = (strcmp(state, "device") == 0);
    free(state);

    return ok;
}
