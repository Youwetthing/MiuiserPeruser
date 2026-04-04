#ifndef SYNDICATE_CAPS_H
#define SYNDICATE_CAPS_H

typedef enum {
    DOMAIN_CPU = 0,    // rocksteadyd
    DOMAIN_DISK,       // metalheadd
    DOMAIN_THERMAL,    // leatherheadd
    DOMAIN_MEM,        // ratkingd
    DOMAIN_BATTERY,    // bebopd
    DOMAIN_SYS,        // krangd
    DOMAIN_ROUTER      // splinterd
} syndicate_domain_t;

typedef struct {
    syndicate_domain_t domain;
    const char *name;
    const char *endpoint; // IPC socket path
} worker_node_t;

#endif
