#ifndef CFG_JSON_H
#define CFG_JSON_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* cfg_json_int: read integer .<section>.<key> from a small JSON file.
 * Returns def if the file/section/key is missing, null, or not a number/bool.
 * true -> 1, false -> 0. No external tools, no popen. File capped at 64 KiB. */
static inline const char *cfg_find_key_(const char *p, const char *end, const char *name)
{
    char pat[96];
    int n = snprintf(pat, sizeof(pat), "\"%s\"", name);
    if (n <= 0 || n >= (int)sizeof(pat)) return NULL;
    size_t pl = (size_t)n;
    for (; p + pl < end; p++) {
        if (*p == '"' && memcmp(p, pat, pl) == 0) {
            const char *q = p + pl;
            while (q < end && isspace((unsigned char)*q)) q++;
            if (q < end && *q == ':') {
                q++;
                while (q < end && isspace((unsigned char)*q)) q++;
                return q;
            }
        }
    }
    return NULL;
}

static inline int cfg_json_int(const char *path, const char *section, const char *key, int def)
{
    FILE *f = fopen(path, "r");
    if (!f) return def;
    char *buf = malloc(65537);
    if (!buf) { fclose(f); return def; }
    size_t n = fread(buf, 1, 65536, f);
    fclose(f);
    buf[n] = '\0';
    const char *end = buf + n;
    int val = def;
    const char *s = cfg_find_key_(buf, end, section);
    if (s && s < end && *s == '{') {
        int depth = 0;
        const char *e = s;
        for (; e < end; e++) {
            if (*e == '{') depth++;
            else if (*e == '}' && --depth == 0) break;
        }
        const char *v = cfg_find_key_(s, e, key);
        if (v && v < end) {
            if (strncmp(v, "true", 4) == 0) val = 1;
            else if (strncmp(v, "false", 5) == 0) val = 0;
            else if (*v == '-' || isdigit((unsigned char)*v)) val = atoi(v);
        }
    }
    free(buf);
    return val;
}
#endif
