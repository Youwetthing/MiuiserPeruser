#include <stdlib.h>
#include <string.h>

#include "backend_thermals.h"
#include "backend_adb.h"
#include "backend_thermals_parse.h"

static int adb_activate(void)
{
    char *devs = backend_adb_exec("devices");
    if (!devs) return 0;
    free(devs);

    char *state = backend_adb_exec("get-state");
    if (!state) return 0;

    int ok = (strcmp(state, "device") == 0);
    free(state);

    return ok;
}

char *backend_thermals(void)
{
    if (!adb_activate())
        return strdup("thermals:adb_not_ready");

    char *raw = backend_adb_exec("dumpsys thermalservice");
    if (!raw)
        return strdup("thermals:error");

    char *parsed = parse_thermalservice_dump(raw);
    free(raw);

    return parsed;
}
