#ifndef CAPABILITIES_H
#define CAPABILITIES_H

#include <stdbool.h>

typedef struct {
    bool adb;
    bool shizuku;
    bool proc_access;
    bool sys_access;
    bool miui_services;
} capability_state_t;

extern capability_state_t capabilities;

void detect_capabilities(void);

#endif
