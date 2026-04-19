#include <stdio.h>
#include <stdarg.h>
void foot_emit_result(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[FOOT_REPORT] ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}
