#ifndef MONITOR_TEMPLATE_H
#define MONITOR_TEMPLATE_H

typedef struct {
    const char *name;
    unsigned int base_sleep;
    char *(*observe)(void);
    const char *log_tag;
} MonitorConfig;

void monitor_run(const MonitorConfig *cfg);

#endif
