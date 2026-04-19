#include "fugitoid_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>

/*
 * foot_blackboxd — Foot Clan Blackbox Logger
 *
 * Purpose:
 *  - Collect logs from all Foot Clan daemons.
 *  - Append them to a persistent blackbox file.
 *  - Rotate logs when they get too large.
 *  - Provide Splinter with a post-mortem trail.
 */

#define BLACKBOX_PATH "/data/data/com.termux/files/home/MiuiserPeruser/foot_blackbox.log"
#define MAX_LOG_SIZE (1024 * 1024)  // 1 MB
#define POLL_INTERVAL_SEC 5

static void rotate_if_needed(void) {
    FILE *f = fopen(BLACKBOX_PATH, "r");
    if (!f) return;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);

    if (size < MAX_LOG_SIZE) return;

    char rotated[512];
    snprintf(rotated, sizeof(rotated),
             "%s.%ld", BLACKBOX_PATH, time(NULL));

    rename(BLACKBOX_PATH, rotated);
}

static void append_log(const char *msg) {
    FILE *f = fopen(BLACKBOX_PATH, "a");
    if (!f) return;

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);

    fprintf(f, "[%04d-%02d-%02d %02d:%02d:%02d] %s\n",
            tm->tm_year + 1900,
            tm->tm_mon + 1,
            tm->tm_mday,
            tm->tm_hour,
            tm->tm_min,
            tm->tm_sec,
            msg);

    fclose(f);
}

int main(void) {
    fugitoid_init();
    fugitoid_log("INFO", "[FOOT BLACKBOX] foot_blackboxd online");

    for (;;) {
        rotate_if_needed();

        // Placeholder: later this will read from a pipe or shared buffer.
        append_log("blackbox tick");

        sleep(POLL_INTERVAL_SEC);
    }

    return 0;
}
