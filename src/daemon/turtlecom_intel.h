#ifndef TURTLECOM_INTEL_H
#define TURTLECOM_INTEL_H

#include <sys/stat.h>
#include <time.h>

#define HEARTBEAT_FILE "/data/data/com.termux/files/home/.syndicate_sewer/turtlecom.heartbeat"

/* Check if the nervous system is responsive */
int is_nerve_system_alive() {
    struct stat st;
    if (stat(HEARTBEAT_FILE, &st) == 0) {
        time_t now = time(NULL);
        // If heartbeat is less than 15 seconds old, we are online
        if ((now - st.st_mtime) < 15) return 1;
    }
    return 0;
}

#endif
