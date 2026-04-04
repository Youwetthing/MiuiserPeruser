#include "service_includes.h"
#include "../core/log_safe.h"
/*
 * MiuiserPeruser – Service management (Termux service wrapper)
 */

int miuiserperuser_service_start(void) {
    pid_t pid = fork();
    if (pid < 0) {
        log_event(LOG_LEVEL_ERROR, "SERVICE", "fork_failed errno=%d", errno);
        return 1;
    }

    if (pid == 0) {
        /* Child – run the main loop */
        log_event(LOG_LEVEL_INFO, "SERVICE", "child_start_main_loop");
        return miuiserperuser_main_loop(false);
    } else {
        /* Parent – wait for child */
        g_child_pid = pid;
        log_event(LOG_LEVEL_INFO, "SERVICE", "child_spawned pid=%d", pid);

        int status;
        waitpid(pid, &status, 0);
        log_event(LOG_LEVEL_INFO, "SERVICE", "child_exited status=%d", status);
        return 0;
    }
}

int miuiserperuser_service_stop(void) {
    if (g_child_pid > 0) {
        kill(g_child_pid, SIGTERM);
        log_event(LOG_LEVEL_INFO, "SERVICE", "child_terminated pid=%d", g_child_pid);
        g_child_pid = 0;
    }
    return 0;
}
