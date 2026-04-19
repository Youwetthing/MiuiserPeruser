#include <stdio.h>
#include <string.h>

#include "doctor_mode.h"

/* Implemented in stubs.c */
void run_single_scan(void);
void run_scheduled_mode(void);
void run_active_mode(void);
void run_selftest(void);
void print_help(void);

int main(int argc, char *argv[]) {

    /* Doctor mode */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--doctor") == 0) {
            run_doctor_mode();
            return 0;
        }
    }

    /* Existing modes */
    for (int i = 1; i < argc; i++) {

        if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            const char *mode = argv[i + 1];

            if (strcmp(mode, "passive") == 0) {
                run_single_scan();
                return 0;
            }

            if (strcmp(mode, "scheduled") == 0) {
                run_scheduled_mode();
                return 0;
            }

            if (strcmp(mode, "active") == 0) {
                run_active_mode();
                return 0;
            }
        }

        if (strcmp(argv[i], "--selftest") == 0) {
            run_selftest();
            return 0;
        }
    }

    /* Default */
    print_help();
    return 0;
}
