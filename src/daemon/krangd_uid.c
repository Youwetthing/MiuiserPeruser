#include "krangd_uid.h"

#include "backend_exec.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int package_character_ok(unsigned char c)
{
    return isalnum(c) || c == '.' || c == '_' || c == '-';
}

parse_status_t krang_parse_package_output(const char *output,
                                          char *package,
                                          size_t package_size)
{
    const char *cursor;

    if (!output || !package || package_size < 2)
        return PARSE_ERROR;
    package[0] = '\0';

    cursor = output;
    while (*cursor) {
        const char *line_end = strchr(cursor, '\n');
        const char *name;
        size_t length;

        while (*cursor == ' ' || *cursor == '\t' ||
               *cursor == '\r')
            cursor++;
        if (strncmp(cursor, "package:", 8) != 0) {
            if (!line_end) break;
            cursor = line_end + 1;
            continue;
        }

        name = cursor + 8;
        while (*name == ' ' || *name == '\t') name++;
        length = 0;
        while (name[length] && name[length] != '\n' &&
               name[length] != '\r' && !isspace((unsigned char)name[length]))
            length++;
        if (length == 0 || length >= package_size)
            return PARSE_ERROR;

        for (size_t i = 0; i < length; i++) {
            if (!package_character_ok((unsigned char)name[i]))
                return PARSE_ERROR;
        }
        memcpy(package, name, length);
        package[length] = '\0';
        return PARSE_FOUND;
    }

    return PARSE_NOT_FOUND;
}

parse_status_t krang_resolve_uid_package(int uid,
                                         char *package,
                                         size_t package_size)
{
    char command[96];
    char *output;
    int written;
    parse_status_t status;

    if (uid < 0 || !package || package_size < 2)
        return PARSE_ERROR;
    package[0] = '\0';

    written = snprintf(command, sizeof(command),
                       "cmd package list packages --uid %d 2>/dev/null",
                       uid);
    if (written < 0 || (size_t)written >= sizeof(command))
        return PARSE_ERROR;

    output = bexec(command);
    if (!output) return PARSE_ERROR;
    status = krang_parse_package_output(output, package, package_size);
    free(output);
    return status;
}