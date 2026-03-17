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

static volatile bool g_running = false;
static bool g_console_mode = false;

/* Logging function – writes to file and optionally console */
static void log_message(const char *level, const char *format, ...) {
    va_list args;
    va_start(args, format);

    time_t now;
    time(&now);
    struct tm *tm = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm);

    FILE *log = fopen("/data/data/com.termux/files/home/miuiserperuser.log", "a");
    if (log) {
        fprintf(log, "[%s] [%s] ", timestamp, level);
        vfprintf(log, format, args);
        fprintf(log, "\n");
        fclose(log);
    }

    if (g_console_mode) {
        printf("[%s] [%s] ", timestamp, level);
        vprintf(format, args);
        printf("\n");
    }

    va_end(args);
}

static void handle_detection(const SENSEI_DETECTION *det, void *user_data) {
    (void)user_data;
    log_message("ALERT", "[%s] %s (score %u)",
                leo_detection_class_to_string(det->detection_class),
                det->description,
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

    signal(SIGINT, miuiserperuser_stop_signal);
    signal(SIGTERM, miuiserperuser_stop_signal);

    log_message("INFO", "MiuiserPeruser daemon starting...");

    if (april_platform_init() != SENSEI_STATUS_OK) {
        log_message("CRIT", "Platform init failed");
        return 1;
    }

    /* Start rish pipe for privileged commands */
    if (rish_pipe_start() == 0) {
        log_message("INFO", "Rish pipe started successfully");
        // Test command – remove later
        char *test = rish_pipe_command("echo hello from rish");
        if (test) {
            log_message("INFO", "Rish test output: %s", test);
            free(test);
        }
    } else {
        log_message("WARN", "Failed to start rish pipe – will fall back to direct forks");
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
        log_message("CRIT", "Detection engine init failed");
        april_platform_cleanup();
        return 1;
    }

    if (miuiserperuser_ipc_init() != SENSEI_STATUS_OK) {
        log_message("WARN", "IPC init failed – continuing without IPC");
    }

    /* Register callback for detection events */
    april_event_register_callback(SENSEI_EVENT_PRIORITY_LOW,
                                  handle_detection, NULL);

    log_message("INFO", "Engine ready. Starting scan loop.");

    SENSEI_DETECTION_LIST results = {0};

    while (g_running) {
        log_message("INFO", "Performing full system scan...");
        leo_full_scan(&results);

        /* Process any queued events (though handle_detection already logs) */
        SENSEI_DETECTION *cur = results.head;
        while (cur) {
            cur = cur->next;
        }

        leo_detection_list_free(&results);
        results.head = NULL;

        /* Sleep in 1‑second chunks to allow quick shutdown */
        for (int i = 0; i < 5 && g_running; i++)
            sleep(1);
    }

    log_message("INFO", "MiuiserPeruser daemon stopping...");
    rish_pipe_stop();
    miuiserperuser_ipc_shutdown();
    leo_shutdown();
    april_platform_cleanup();

    return 0;
}
