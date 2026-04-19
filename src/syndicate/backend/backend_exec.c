#include <stdio.h>
#include <stdlib.h>

static const char *WRAPPER = "rish -c";

char* backend_exec(const char *cmd) {
    char full[512];
    snprintf(full, sizeof(full), "%s '%s'", WRAPPER, cmd);

    FILE *fp = popen(full, "r");
    if (!fp) return NULL;

    char *buf = malloc(2048);
    if (!buf) {
        pclose(fp);
        return NULL;
    }

    size_t len = fread(buf, 1, 2047, fp);
    buf[len] = '\0';

    pclose(fp);
    return buf;
}
