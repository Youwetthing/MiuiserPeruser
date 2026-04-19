#ifndef SPLINTER_SELECTOR_H
#define SPLINTER_SELECTOR_H

typedef enum {
    BACKEND_NONE = 0,
    BACKEND_SYSFS,
    BACKEND_RISH,
    BACKEND_ADB,
    BACKEND_PORTBRIDGE,
} backend_kind_t;

backend_kind_t splinter_pick_backend(void);
const char *splinter_backend_name(backend_kind_t b);

#endif
