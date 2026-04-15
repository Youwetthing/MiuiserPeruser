#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "superhero_mode.h"
#include "leo_detection.h"

int main(int argc, char **argv) {
    leo_init(NULL);

    if (argc > 1) {
        if (strcmp(argv[1], "--loop") == 0) {
            uint32_t interval = (argc > 2) ? (uint32_t)atoi(argv[2]) : 5;
            int ret = (superhero_run_loop(interval) == 0) ? 0 : 1;
            leo_shutdown();
            return ret;
        }
        if (strcmp(argv[1], "--n") == 0 && argc > 2) {
            uint32_t count = (uint32_t)atoi(argv[2]);
            uint32_t interval = (argc > 3) ? (uint32_t)atoi(argv[3]) : 5;
            int ret = (superhero_run_n_times(count, interval) == 0) ? 0 : 1;
            leo_shutdown();
            return ret;
        }
    }

    int ret = (superhero_run_once() == 0) ? 0 : 1;
    leo_shutdown();
    return ret;
}
