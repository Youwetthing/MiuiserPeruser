#ifndef SPLINTER_SELECTOR_H
#define SPLINTER_SELECTOR_H

// Splinter's view of all possible backends
typedef enum {
    BACKEND_NONE = 0,
    BACKEND_SYSFS,
    BACKEND_RISH,
    BACKEND_ADB,
    BACKEND_PORTBRIDGE,
} backend_kind_t;

// Splinter chooses the best backend based on diagnostics
backend_kind_t splinter_pick_backend(void);

// Human-readable backend name (for logs, doctor output, etc.)
const char *splinter_backend_name(backend_kind_t b);

#endif // SPLINTER_SELECTOR_H

