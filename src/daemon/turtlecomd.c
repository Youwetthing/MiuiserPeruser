#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "ipc_globals.h"
#include <sys/file.h>

#define TURTLE_PID_PATH      BASE "/pipes/pids/turtlecomd.pid"
#define TURTLE_TCP_DISCOVERY MP_PIPES_DIR "/turtlecomd.tcp"
#define UNIX_RETRY_INTERVAL_SEC 30

typedef enum { TRANSPORT_UNIX, TRANSPORT_TCP } transport_t;

static volatile sig_atomic_t g_tc_running = 1;
static FILE *g_log_fp = NULL;
static int g_serv_fd = -1;
static transport_t g_transport = TRANSPORT_UNIX;

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
    if (g_serv_fd >= 0) shutdown(g_serv_fd, SHUT_RDWR);
}

/* ── Unix domain socket (primary transport) ─────────────────────────────
 * Returns a bound, listening fd on success, or -1 on any failure. Never
 * leaves a partially-set-up fd open on the failure path. */
static int bind_unix_socket(void) {
    unlink(TURTLE_SOCKET);

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        turtlelog("ERROR", "unix socket() failed: %s", strerror(errno));
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, TURTLE_SOCKET, sizeof(addr.sun_path) - 1);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        turtlelog("ERROR", "unix bind failed on %s: %s", TURTLE_SOCKET, strerror(errno));
        close(fd);
        return -1;
    }

    chmod(TURTLE_SOCKET, 0600);

    if (listen(fd, 10) < 0) {
        turtlelog("ERROR", "unix listen failed: %s", strerror(errno));
        close(fd);
        unlink(TURTLE_SOCKET);
        return -1;
    }

    return fd;
}

/* ── TCP loopback (fallback transport) ──────────────────────────────────
 * Binds to an ephemeral port on 127.0.0.1 and publishes the assigned port
 * to a discovery file so a future client library can find it. Returns a
 * bound, listening fd on success, or -1 on any failure. */
static int bind_tcp_fallback(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        turtlelog("ERROR", "tcp socket() failed: %s", strerror(errno));
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0; /* ephemeral -- kernel assigns, avoids port squatting */

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        turtlelog("ERROR", "tcp bind failed: %s", strerror(errno));
        close(fd);
        return -1;
    }

    socklen_t alen = sizeof(addr);
    if (getsockname(fd, (struct sockaddr *)&addr, &alen) < 0) {
        turtlelog("ERROR", "getsockname failed: %s", strerror(errno));
        close(fd);
        return -1;
    }
    int port = ntohs(addr.sin_port);

    FILE *df = fopen(TURTLE_TCP_DISCOVERY, "w");
    if (!df) {
        turtlelog("ERROR", "cannot write TCP discovery file %s: %s",
                  TURTLE_TCP_DISCOVERY, strerror(errno));
        close(fd);
        return -1;
    }
    fprintf(df, "%d\n", port);
    fclose(df);
    chmod(TURTLE_TCP_DISCOVERY, 0600);

    if (listen(fd, 10) < 0) {
        turtlelog("ERROR", "tcp listen failed: %s", strerror(errno));
        close(fd);
        unlink(TURTLE_TCP_DISCOVERY);
        return -1;
    }

    turtlelog("WARNING", "unix socket unavailable -- falling back to TCP loopback port=%d", port);
    return fd;
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

    g_serv_fd = bind_unix_socket();
    if (g_serv_fd >= 0) {
        g_transport = TRANSPORT_UNIX;
        turtlelog("INFO", "on the air -- pid=%d transport=unix socket=%s", getpid(), TURTLE_SOCKET);
    } else {
        g_serv_fd = bind_tcp_fallback();
        if (g_serv_fd < 0) {
            turtlelog("ERROR", "both unix and tcp transports failed to bind -- exiting");
            return 1;
        }
        g_transport = TRANSPORT_TCP;
        turtlelog("INFO", "on the air -- pid=%d transport=tcp (fallback)", getpid());
    }

    char buf[1024];
    time_t last_retry = time(NULL);

    while (g_tc_running) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(g_serv_fd, &rfds);
        struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };

        int rv = select(g_serv_fd + 1, &rfds, NULL, NULL, &tv);
        if (rv < 0) {
            if (errno == EINTR) continue;
            turtlelog("ERROR", "select() failed: %s", strerror(errno));
            break;
        }

        if (rv > 0 && FD_ISSET(g_serv_fd, &rfds)) {
            int conn_fd = accept(g_serv_fd, NULL, NULL);
            if (conn_fd >= 0) {
                ssize_t n = read(conn_fd, buf, sizeof(buf) - 1);
                if (n > 0) {
                    buf[n] = 0;
                    turtlelog("DEBUG", "recv: %s", buf);
                }
                close(conn_fd);
            }
        }

        /* Background reconciliation: only relevant while degraded to TCP.
         * Never disrupts an in-progress accept/read cycle above -- this
         * only fires between select() timeouts. */
        if (g_transport == TRANSPORT_TCP) {
            time_t now = time(NULL);
            if (now - last_retry >= UNIX_RETRY_INTERVAL_SEC) {
                last_retry = now;
                int unix_fd = bind_unix_socket();
                if (unix_fd >= 0) {
                    turtlelog("INFO", "unix socket recovered -- switching back from TCP fallback");
                    close(g_serv_fd);
                    unlink(TURTLE_TCP_DISCOVERY);
                    g_serv_fd = unix_fd;
                    g_transport = TRANSPORT_UNIX;
                }
            }
        }
    }

    turtlelog("INFO", "signing off");
    close(g_serv_fd);
    if (g_transport == TRANSPORT_UNIX) {
        unlink(TURTLE_SOCKET);
    } else {
        unlink(TURTLE_TCP_DISCOVERY);
    }
    if (g_lock_fd >= 0) {
        flock(g_lock_fd, LOCK_UN);
        close(g_lock_fd);
    }
    unlink(TURTLE_PID_PATH);
    if (g_log_fp) fclose(g_log_fp);
    return 0;
}
