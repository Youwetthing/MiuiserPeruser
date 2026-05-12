#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "backend_adb.h"

char *backend_adb_exec(const char *cmd)
{
    if (!cmd || strlen(cmd) == 0)
        return strdup("adb:error");

    char fullcmd[256];
    snprintf(fullcmd, sizeof(fullcmd),
             "adb shell \"%s\" 2>/dev/null", cmd);

    FILE *fp = popen(fullcmd, "r");
    if (!fp)
        return strdup("adb:error");

    char buf[256];
    char *out = malloc(4096);
    if (!out) {
        pclose(fp);
        return strdup("adb:error");
    }

    out[0] = '\0';

    while (fgets(buf, sizeof(buf), fp)) {
        strncat(out, buf, 4095 - strlen(out));
    }

    pclose(fp);

    if (strlen(out) == 0)
        return strdup("adb:empty");

    out[strcspn(out, "\n")] = '\0';

    return out;
}
