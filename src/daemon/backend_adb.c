#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "backend_adb.h"

/*
 * Wrap cmd in single quotes for the local shell, escaping embedded quotes.
 * Returns 0 on success, -1 if the quoted form does not fit in out.
 */
static int shell_quote(const char *cmd, char *out, size_t out_len)
{
    size_t o = 0;

    if (out_len < 3) return -1;
    out[o++] = '\'';

    for (const char *p = cmd; *p; p++) {
        if (*p == '\'') {
            if (o + 4 >= out_len) return -1;
            memcpy(out + o, "'\\''", 4);
            o += 4;
        } else {
            if (o + 1 >= out_len) return -1;
            out[o++] = *p;
        }
    }

    if (o + 2 > out_len) return -1;
    out[o++] = '\'';
    out[o] = '\0';
    return 0;
}

char *backend_adb_exec(const char *cmd)
{
    if (!cmd || strlen(cmd) == 0)
        return strdup("adb:error");

    char quoted[256];
    if (shell_quote(cmd, quoted, sizeof(quoted)) != 0)
        return strdup("adb:error");

    char fullcmd[320];
    snprintf(fullcmd, sizeof(fullcmd),
             "adb shell %s 2>/dev/null", quoted);

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
