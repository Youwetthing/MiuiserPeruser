#include <stdio.h>
#include <stdbool.h>
#include "splinter_protocol.h"
#include "capabilities_extra.h"
#include "port_pathway.h"

/* Bring in the global capability state */
extern struct capabilities_state capabilities;

/* Pretty printer implemented elsewhere */
void print_capabilities_pretty(void);

/* Summary + hints implemented elsewhere */
void capability_print_summary(void);
void capability_print_hints(void);

void run_doctor_mode(void) {
    printf("\n=== MiuiserPeruser Doctor Mode ===\n");

    printf("\n[1] Capability scan]\n");
    detect_capabilities();

    print_capabilities_pretty();

    printf("\n[2] Port Bridge Analysis]\n");

if (splinter_protocol_probe()) {
    printf("  Port bridge detected on 127.0.0.1:%d\n", MP_TCP_PORT);

    if (splinter_protocol_basic_info()) {
        printf("  Bridge responded with info.\n");
    } else {
        printf("  Bridge responded but did not return info.\n");
    }
} else {
    printf("  No port bridge detected.\n");
}

    printf("\n[3] Summary]\n");
    capability_print_summary();

    printf("\n[4] Recommendations]\n");
    capability_print_hints();

    printf("\n=== Doctor Mode Complete ===\n\n");
}
