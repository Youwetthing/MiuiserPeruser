#ifndef BACKEND_COMMON_H
#define BACKEND_COMMON_H

#include <stdbool.h>

typedef enum {
    BACKEND_NONE = 0,
    BACKEND_APRIL,
    BACKEND_RISH,
    BACKEND_SHIZUKU,
    BACKEND_ADB,
    BACKEND_SYSPORT,
    BACKEND_TERMUX_FALLBACK
} backend_type_t;

typedef struct {
    backend_type_t type;
    const char *name;
    bool privileged;
    bool via_network;
} backend_info_t;

typedef struct {
    int (*init)(void);
    int (*shutdown)(void);
    int (*read_file)(const char *path, char *buf, int buf_size);
    int (*run_command)(const char *cmd, char *buf, int buf_size);
} backend_vtable_t;

const backend_info_t *backend_get_active_info(void);
const backend_vtable_t *backend_get_active_vtable(void);

int backend_select_best(void);

#endif
