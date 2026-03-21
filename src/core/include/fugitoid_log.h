#ifndef FUGITOID_LOG_H
#define FUGITOID_LOG_H

void fugitoid_init(const char *name);
void fugitoid_log(const char *level, const char *fmt, ...);

#endif
/* Structured JSON logger */
void fugitoid_log_json(const char *level, const char *domain, const char *component,
                       const char *event, const char *correlation_id, const char *msg, const char *meta_json);
