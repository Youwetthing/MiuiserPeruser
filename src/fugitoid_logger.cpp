#include "fugitoid_logger.h"
#include <cstdio>
#include <cstdarg>

void fugitoid_log(const char* tag, const char* fmt, ...) {
    printf("[%s] ", tag);

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    printf("\n");
}
