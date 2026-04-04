#ifndef FUGITOID_LOG_H
#define FUGITOID_LOG_H

void fugitoid_init(void);
void fugitoid_log(const char *tag, const char *fmt, ...);

/* 7‑argument JSON logger used by log_safe.c and resource_snapshot.c */
void fugitoid_log_json(
    const char *level,
    const char *domain,
    const char *component,
    const char *tag,
    const char *correlation_id,
    const char *message,
    const char *meta_json
);

#endif
