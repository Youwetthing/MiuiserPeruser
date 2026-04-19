#define LIB_PATH "/data/data/com.termux/files/home/MiuiserPeruser/lib/miuiserperuser_common.sh"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *run_shell_cmd(const char *cmd) {
    if (!cmd) return NULL;

    // Escape double quotes
    char escaped[1024];
    const char *src = cmd;
    char *dst = escaped;
    while (*src && (dst - escaped) < 1000) {
        if (*src == '"') *dst++ = '\\';
        *dst++ = *src++;
    }
    *dst = '\0';

    // Full command with explicit bash sourcing and PATH
    char full_cmd[2048];
    snprintf(full_cmd, sizeof(full_cmd),
             "export PATH=$PATH:$HOME/.shizuku:$PREFIX/bin && "
             "source $HOME/MiuiserPeruser/lib/miuiserperuser_common.sh && "
             "run_shell \"%s\" 2>/dev/null",
             escaped);

    FILE *fp = popen(full_cmd, "r");
    if (!fp) return NULL;

    size_t size = 8192;
    char *output = malloc(size);
    if (!output) {
        pclose(fp);
        return NULL;
    }
    output[0] = '\0';

    size_t pos = 0;
    char buf[256];
    while (fgets(buf, sizeof(buf), fp)) {
        size_t len = strlen(buf);
        if (pos + len + 1 >= size) {
            size *= 2;
            char *tmp = realloc(output, size);
            if (!tmp) break;
            output = tmp;
        }
        strcpy(output + pos, buf);
        pos += len;
    }
    pclose(fp);
    return output;
}
