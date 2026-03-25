#include <stdio.h>
#include <stdarg.h>
#include "fugitoid_log.h"

void fugitoid_init(void) {
    printf("[FUGITOID] init\n");
}

void fugitoid_log(const char *tag, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    printf("[%s] ", tag);
    vprintf(fmt, args);
    printf("\n");

    va_end(args);
}

/* 7‑argument JSON logger — matches core usage */
void fugitoid_log_json(
    const char *level,
    const char *domain,
    const char *component,
    const char *tag,
    const char *correlation_id,
    const char *message,
    const char *meta_json
) {
    printf("[%s] { \"domain\":\"%s\", \"component\":\"%s\", \"tag\":\"%s\", \"correlation_id\":\"%s\", \"message\":\"%s\", \"meta\":%s }\n",
        level,
        domain,
        component,
        tag,
        correlation_id ? correlation_id : "",
        message,
        meta_json
    );
}
