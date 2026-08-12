#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include "ipc_globals.h"
#include <sys/file.h>

#define TURTLE_PID_PATH  BASE "/pipes/pids/turtlecomd.pid"

static volatile sig_atomic_t g_tc_running = 1;
static FILE *g_log_fp = NULL;
static int g_serv_fd = -1;

static void turtlelog(const char *level, const char *fmt, ...) {
    time_t t = time(NULL);
    char tsbuf[32];
    strftime(tsbuf, sizeof(tsbuf), "%Y-%m-%dT%H:%M:%S", localtime(&t));

    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[%s][TURTLECOMD/%s] ", tsbuf, level);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);

    if (g_log_fp) {
        va_start(ap, fmt);
        fprintf(g_log_fp, "[%s][TURTLECOMD/%s] ", tsbuf, level);
        vfprintf(g_log_fp, fmt, ap);
        fprintf(g_log_fp, "\n");
        fflush(g_log_fp);
        va_end(ap);
    }
}

static int g_lock_fd = -1;

static int acquire_singleton_lock(void) {
    g_lock_fd = open(TURTLE_PID_PATH, O_CREAT | O_RDWR, 0644);
    if (g_lock_fd < 0) {
        turtlelog("ERROR", "cannot open pidfile for locking: %s", TURTLE_PID_PATH);
        return -1;
    }
    if (flock(g_lock_fd, LOCK_EX | LOCK_NB) < 0) {
        turtlelog("ERROR", "another turtlecomd instance is already running (lock held on %s)", TURTLE_PID_PATH);
        close(g_lock_fd);
        g_lock_fd = -1;
        return -1;
    }
    /* Lock acquired -- now safe to write our PID */
    char buf[32];
    int n = snprintf(buf, sizeof(buf), "%d\n", getpid());
    if (ftruncate(g_lock_fd, 0) < 0) { /* best-effort clear stale content */ }
    lseek(g_lock_fd, 0, SEEK_SET);
    write(g_lock_fd, buf, (size_t)n);
    return 0;
}

static void handle_shutdown(int sig) {
    (void)sig;
    g_tc_running = 0;
}

int main(void) {
    signal(SIGINT, handle_shutdown);
    signal(SIGTERM, handle_shutdown);

    const char *log_path = getenv("TURTLECOMD_LOG_PATH");
    if (log_path) g_log_fp = fopen(log_path, "a");

    if (acquire_singleton_lock() < 0) {
        return 1;
    }

    mkdir(MP_PIPES_DIR, 0777);
    unlink(TURTLE_SOCKET);

    g_serv_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_serv_fd < 0) {
        turtlelog("ERROR", "socket() failed");
        return 1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, TURTLE_SOCKET, sizeof(addr.sun_path) - 1);

    if (bind(g_serv_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        turtlelog("ERROR", "bind failed on %s", TURTLE_SOCKET);
        return 1;
    }

    chmod(TURTLE_SOCKET, 0666);
    listen(g_serv_fd, 10);
    turtlelog("INFO", "on the air -- pid=%d socket=%s", getpid(), TURTLE_SOCKET);

    char buf[1024];
    while (g_tc_running) {
        int conn_fd = accept(g_serv_fd, NULL, NULL);
        if (conn_fd < 0) {
            if (!g_tc_running) break;
            continue;
        }
        ssize_t n = read(conn_fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = 0;
            turtlelog("DEBUG", "recv: %s", buf);
        }
        close(conn_fd);
    }

    turtlelog("INFO", "signing off");
    close(g_serv_fd);
    unlink(TURTLE_SOCKET);
    if (g_lock_fd >= 0) {
        flock(g_lock_fd, LOCK_UN);
        close(g_lock_fd);
    }
    unlink(TURTLE_PID_PATH);
    if (g_log_fp) fclose(g_log_fp);
    return 0;
}
