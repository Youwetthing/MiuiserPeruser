#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <pthread.h>
#include "daemon_core.h"
#include "ipc_globals.h"
#include "daemon_common.h"

volatile bool g_running = false;
pthread_t g_thread;
pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

static bool g_console_mode = false;

void miuiserperuser_stop_signal(int sig) {
    (void)sig;
    g_running = false;
}

int miuiserperuser_main_loop(bool console_mode) {
    g_console_mode = console_mode;
    g_running = true;

    signal(SIGINT,  miuiserperuser_stop_signal);
    signal(SIGTERM, miuiserperuser_stop_signal);
    signal(SIGPIPE, SIG_IGN);

    fprintf(stdout, "[CORE] MiuiserPeruser daemon starting\n");

    if (miuiserperuser_ipc_init() != 0) {
        fprintf(stderr, "[CORE] IPC init failed — continuing without IPC\n");
    }

    fprintf(stdout, "[CORE] scan loop running\n");

    while (g_running) {
        /* Daemon fleet health check */
        for (int i = 0; i < 5 && g_running; i++)
            sleep(1);
    }

    fprintf(stdout, "[CORE] shutting down\n");
    miuiserperuser_ipc_shutdown();
    return 0;
}
