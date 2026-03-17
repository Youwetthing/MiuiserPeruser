/*
 * MiuiserPeruser – Service management (Termux service wrapper)
 */

#include "service.h"
#include "daemon_core.h"
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

static pid_t g_child_pid = 0;

static void sigterm_handler(int sig) {
    (void)sig;
    if (g_child_pid > 0)
        kill(g_child_pid, SIGTERM);
}

int miuiserperuser_service_start(void) {
    signal(SIGTERM, sigterm_handler);
    signal(SIGINT, sigterm_handler);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }
    if (pid == 0) {
        /* Child – run the main loop */
        return miuiserperuser_main_loop(false);
    } else {
        /* Parent – wait for child */
        g_child_pid = pid;
        int status;
        waitpid(pid, &status, 0);
        return 0;
    }
}

int miuiserperuser_service_stop(void) {
    if (g_child_pid > 0) {
        kill(g_child_pid, SIGTERM);
        g_child_pid = 0;
    }
    return 0;
}
