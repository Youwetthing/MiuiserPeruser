#include "monitor_template.h"
#include "april_table.h"
#include "syndicate_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

static volatile int _running = 1;
static void _cleanup(int sig) { _running = 0; }

void monitor_run(const MonitorConfig *cfg) {
    signal(SIGINT,  _cleanup);
    signal(SIGTERM, _cleanup);
    syndicate_init();
    printf("[%s] ONLINE\n", cfg->name);

    while (_running) {
        if (april_read(APRIL_SYSTEM_LOCK, SYSLOCK_NORMAL) == SYSLOCK_LOCKED) {
            sleep(april_poll_sleep(cfg->base_sleep));
            continue;
        }
        uint32_t log_level = april_read(APRIL_LOG_LEVEL, LOG_NORMAL);
        char *data = cfg->observe();
        if (log_level >= LOG_NORMAL) {
            log_cabin(cfg->name, data ? data : "empty");
            db_log(cfg->name, cfg->log_tag, data ? data : "empty");
        }
        if (log_level == LOG_VERBOSE && data)
            printf("[%s] %s\n", cfg->name, data);
        free(data);
        sleep(april_poll_sleep(cfg->base_sleep));
    }
    printf("[%s] shutdown\n", cfg->name);
}
