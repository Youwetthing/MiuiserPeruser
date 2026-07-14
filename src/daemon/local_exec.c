/*
 * local_exec.c — Simple command execution, no privilege required
 *
 * Runs a shell command via popen(), captures stdout, returns as
 * malloc'd string. Caller must free(). Returns NULL on failure.
 */

#define _GNU_SOURCE
#include "local_exec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *local_exec(const char *cmd)
{
    FILE *f = popen(cmd, "r");
    if (!f) return NULL;

    size_t cap = 256;
    size_t len = 0;
    char  *buf = malloc(cap);
    if (!buf) { pclose(f); return NULL; }
    buf[0] = '\0';

    char tmp[256];
    while (fgets(tmp, sizeof(tmp), f)) {
        size_t n = strlen(tmp);
        if (len + n + 1 > cap) {
            cap = cap * 2 + n + 1;
            char *nb = realloc(buf, cap);
            if (!nb) { free(buf); pclose(f); return NULL; }
            buf = nb;
        }
        memcpy(buf + len, tmp, n);
        len += n;
        buf[len] = '\0';
    }

    int status = pclose(f);
    if (status != 0 && len == 0) {
        free(buf);
        return NULL;
    }

    /* Trim trailing whitespace (portscan prints "\n" after the port) */
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r' || buf[len - 1] == ' ')) {
        buf[--len] = '\0';
    }

    return buf;
}
