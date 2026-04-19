#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "backend_thermals_parse.h"

// Extracts mValue=XX.X and mName=XXXX from a line
static int parse_line(const char *line, char *name_out, float *value_out)
{
    const char *v = strstr(line, "mValue=");
    const char *n = strstr(line, "mName=");

    if (!v || !n)
        return 0;

    *value_out = strtof(v + 7, NULL);

    n += 6;
    int i = 0;
    while (*n && *n != ',' && *n != '}' && i < 31)
        name_out[i++] = *n++;
    name_out[i] = '\0';

    return 1;
}

char *parse_thermalservice_dump(const char *dump)
{
    float cpu = -1, gpu = -1, batt = -1, skin = -1, soc = -1;

    char *copy = strdup(dump);
    if (!copy) return strdup("thermals:parse_error");

    char *line = strtok(copy, "\n");
    while (line) {
        char name[32];
        float val = 0;

        if (parse_line(line, name, &val)) {
            if (strcasecmp(name, "CPU") == 0) cpu = val;
            else if (strcasecmp(name, "GPU") == 0) gpu = val;
            else if (strcasecmp(name, "BATTERY") == 0) batt = val;
            else if (strcasecmp(name, "SKIN") == 0) skin = val;
            else if (strcasecmp(name, "SOC") == 0) soc = val;
        }

        line = strtok(NULL, "\n");
    }

    free(copy);

    char *out = malloc(256);
    if (!out) return strdup("thermals:parse_error");

    snprintf(out, 256,
             "cpu=%.1f gpu=%.1f battery=%.1f skin=%.1f soc=%.1f",
             cpu, gpu, batt, skin, soc);

    return out;
}
