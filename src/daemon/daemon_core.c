#include "daemon_core.h"

#include <stdio.h>
#include <stdarg.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

static const char *g_daemon_name = "daemon";
static int g_pid_fd = -1;

/* ------------------------------
 * Internal helpers
 * ------------------------------ */

static void handle_signal(int sig) {
    daemon_log_info("received signal %d, shutting down", sig);
    daemon_core_shutdown();
    _exit(0);
}

static void install_signal_handlers(void) {
    struct sigaction sa = {0};
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

/* Create /tmp/<daemon>.pid */
static bool create_pid_file(void) {
    char path[256];
    snprintf(path, sizeof(path), "/data/data/com.termux/files/home/tmp/%s.pid", g_daemon_name);

    g_pid_fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (g_pid_fd < 0) {
        daemon_log_error("failed to create pid file: %s", path);
        return false;
    }

    dprintf(g_pid_fd, "%d\n", getpid());
    return true;
}

/* Ensure /tmp exists (MIUI sometimes nukes it) */
static void ensure_tmp_exists(void) {
    mkdir("/tmp", 0755);
}

/* ------------------------------
 * Public API
 * ------------------------------ */

bool daemon_core_init(const char *daemon_name) {
    g_daemon_name = daemon_name;

    ensure_tmp_exists();
    install_signal_handlers();

    if (!create_pid_file()) {
        return false;
    }

    daemon_log_info("initialised");
    return true;
}

void daemon_core_shutdown(void) {
    if (g_pid_fd >= 0) {
        close(g_pid_fd);
        g_pid_fd = -1;
    }

    daemon_log_info("shutdown complete");
}

/* ------------------------------
 * Logging wrappers
 * ------------------------------ */

static void vlog(const char *level, const char *fmt, va_list ap) {
    fprintf(stderr, "[%s] %s: ", level, g_daemon_name);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
}

void daemon_log_info(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vlog("INFO", fmt, ap);
    va_end(ap);
}

void daemon_log_error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vlog("ERROR", fmt, ap);
    va_end(ap);
}

void daemon_log_debug(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vlog("DEBUG", fmt, ap);
    va_end(ap);
}
