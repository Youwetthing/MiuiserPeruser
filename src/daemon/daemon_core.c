#include "daemon_core_includes.h"
#include "../core/log_safe.h"
/* Single static prototype for heartbeat handler */
static void handle_rocksteady_heartbeat(const char *line);

/*
 * MiuiserPeruser – Daemon core implementation with rish pipe
 */

#include "daemon_core.h"
#include "ipc.h"
#include "../core/include/leo_detection.h"
#include "../core/include/april_platform.h"
#include "rish_pipe.h"
#include "../core/include/april_event.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>
#include <stdbool.h>

static volatile bool g_running = false;
static bool g_console_mode = false;

/* Detection callback – logs summary only, then broadcasts via IPC */
static void handle_detection(const SENSEI_DETECTION *det, void *user_data) {
    (void)user_data;

    log_event(LOG_LEVEL_WARN,
              "DETECTION",
              "detection class=%s score=%u",
              leo_detection_class_to_string(det->detection_class),
              leo_calculate_score(det));

    miuiserperuser_ipc_broadcast(det);
}

void miuiserperuser_stop_signal(int sig) {
    (void)sig;
    g_running = false;
}

int miuiserperuser_main_loop(bool console_mode) {
    g_console_mode = console_mode;
    g_running = true;

    /* Unified logging: sensei.log */
    log_init("sensei");
    log_set_level(console_mode ? LOG_LEVEL_DEBUG : LOG_LEVEL_INFO);

    signal(SIGINT, miuiserperuser_stop_signal);
    signal(SIGTERM, miuiserperuser_stop_signal);
    signal(SIGPIPE, SIG_IGN);

    log_event(LOG_LEVEL_INFO, "CORE", "daemon_start");

    if (april_platform_init() != SENSEI_STATUS_OK) {
        log_event(LOG_LEVEL_ERROR, "CORE", "platform_init_failed");
        return 1;
    }

    /* Start rish pipe for privileged commands */
    if (rish_pipe_start() == 0) {
        log_event(LOG_LEVEL_INFO, "RISH", "rish_pipe_start_success");
    } else {
        log_event(LOG_LEVEL_WARN, "RISH", "rish_pipe_start_failed_fallback");
    }

    SENSEI_DETECTION_CONFIG config = {
        .enable_memory_scan = true,
        .enable_hook_detection = true,
        .enable_behavior_analysis = true,
        .enable_kernel_analysis = false,
        .enable_network_monitor = true,
        .enable_integrity_monitor = true,
        .scan_interval_ms = 5000
    };

    if (leo_init(&config) != SENSEI_STATUS_OK) {
        log_event(LOG_LEVEL_ERROR, "CORE", "detection_engine_init_failed");
        april_platform_cleanup();
        return 1;
    }

    if (miuiserperuser_ipc_init() != SENSEI_STATUS_OK) {
        log_event(LOG_LEVEL_WARN, "IPC", "ipc_init_failed_continue_without_ipc");
    }

    /* Register callback for detection events */
    april_event_register_callback(SENSEI_EVENT_PRIORITY_LOW,
                                  handle_detection, NULL);

    log_event(LOG_LEVEL_INFO, "CORE", "scan_loop_start");

    SENSEI_DETECTION_LIST results = {0};

    while (g_running) {
        log_event(LOG_LEVEL_DEBUG, "SCAN", "scan_start");
        leo_full_scan(&results);

        /* Process any queued events (though handle_detection already logs) */
        SENSEI_DETECTION *cur = results.head;
        while (cur) {
            cur = cur->next;
        }

        leo_detection_list_free(&results);
        results.head = NULL;

        log_event(LOG_LEVEL_DEBUG, "SCAN", "scan_end");

        /* Sleep in 1‑second chunks to allow quick shutdown */
        for (int i = 0; i < 5 && g_running; i++)
            sleep(1);
    }

    log_event(LOG_LEVEL_INFO, "CORE", "daemon_stop");
    rish_pipe_stop();
    miuiserperuser_ipc_shutdown();
    leo_shutdown();
    april_platform_cleanup();

    return 0;
}

/* Rocksteady heartbeat handler stub – to be wired to IPC/rish as needed */
static void handle_rocksteady_heartbeat(const char *line) {
    (void)line;
    log_event(LOG_LEVEL_DEBUG, "CORE", "rocksteady_heartbeat");
}
