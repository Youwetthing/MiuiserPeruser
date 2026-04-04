#include "../sensei_core.h"
#include <string.h>
#include <ctype.h>

/*
 * Protocol Cortex — real parser v1
 *
 * Rules:
 *   - First token = command
 *   - Remaining tokens = args
 *   - Max 8 args
 *   - Max command length 63
 *   - Max arg length 127
 */

static void trim(char *s) {
    char *end;
    while (isspace((unsigned char)*s)) s++;
    if (*s == 0) return;
    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
}

bool cortex_protocol_parse(const char *raw, sensei_msg_t *out) {
    if (!raw || !out) return false;

    memset(out, 0, sizeof(*out));
    strncpy(out->raw, raw, sizeof(out->raw) - 1);

    char buf[1024];
    strncpy(buf, raw, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    trim(buf);
    if (buf[0] == '\0') return false;

    char *tok = strtok(buf, " ");
    if (!tok) return false;

    strncpy(out->command, tok, sizeof(out->command) - 1);

    int argc = 0;
    while ((tok = strtok(NULL, " ")) != NULL && argc < 8) {
        strncpy(out->args[argc], tok, sizeof(out->args[argc]) - 1);
        argc++;
    }

    out->argc = argc;
    return true;
}

void cortex_protocol_free_msg(sensei_msg_t *msg) {
    (void)msg;
}
