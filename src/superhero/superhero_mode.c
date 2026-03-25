#include "superhero/superhero_mode.h"
#include "backends/backend_common.h"
// later: turtles, splinter, fugitoid, krang, etc.

#include <stdio.h>

int superhero_run_full_scan(void) {
    if (backend_select_best() != 0) {
        fprintf(stderr, "[superhero] failed to select backend\n");
        return -1;
    }

    const backend_info_t *info = backend_get_active_info();
    fprintf(stdout, "[superhero] using backend: %s (privileged=%d)\n",
            info->name, info->privileged);

    // TODO:
    //  - run turtles
    //  - run Footrunner jobs
    //  - query sysport
    //  - populate Fugitoid snapshot
    //  - send via Splinter
    //  - expose via Turtlecom

    return 0;
}
