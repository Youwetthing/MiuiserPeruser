#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include "ipc_globals.h"

#define LOG_FILE  BASE "/Log_Cabin/krangd.log"
#define PID_FILE  BASE "/pipes/pids/krangd.pid"
#define LOG_CAP   (1 * 1024 * 1024)
#define BUF_SIZE  512

/* april.bin offsets */
#define OFF_KRANG_MODE    4
#define OFF_LOG_LEVEL     8
#define OFF_TCP_FALLBACK  16
#define OFF_SYSTEM_LOCK   20
#define OFF_POLL_THROTTLE 24
#define OFF_SCAN_EPOCH    28

#define APRIL_BIN BASE "/Database/april.bin"

static volatile sig_atomic_t g_running = 1;
static int g_server_fd = -1;

/* ── april.bin ───────────────────────────────────────────── */
static int april_read(int offset) {
    int fd = open(APRIL_BIN, O_RDONLY);
    if (fd < 0) return 0;
    int val = 0;
    pread(fd, &val, sizeof(int), offset);
    close(fd);
    return val;
}

static void april_write(int offset, int value) {
    int fd = open(APRIL_BIN, O_WRONLY);
    if (fd < 0) return;
    pwrite(fd, &value, sizeof(int), offset);
    close(fd);
}

/* ── Logging ─────────────────────────────────────────────── */
static void klog(int level, const char *msg) {
    if (level > april_read(OFF_LOG_LEVEL)) return;
    struct stat st;
    if (stat(LOG_FILE, &st) == 0 && st.st_size >= LOG_CAP)
        rename(LOG_FILE, BASE "/Log_Cabin/krangd.log.old");
    FILE *f = fopen(LOG_FILE, "a");
    if (!f) return;
    time_t now = time(NULL);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(f, "[%s] [krangd] [L%d] %s\n", ts, level, msg);
    fclose(f);
}

/* ── Signal handler ──────────────────────────────────────── */
static void handle_sig(int sig) {
    (void)sig;
    g_running = 0;
    if (g_server_fd >= 0) {
        shutdown(g_server_fd, SHUT_RDWR);
        close(g_server_fd);
        g_server_fd = -1;
    }
}

/* ── PID file ────────────────────────────────────────────── */
static void write_pid(void) {
    mkdir(BASE "/pipes/pids", 0700);
    FILE *f = fopen(PID_FILE, "w");
    if (!f) return;
    fprintf(f, "%d\n", (int)getpid());
    fclose(f);
}

/* ── SCAN_EPOCH tick ─────────────────────────────────────── */
static void bump_epoch(void) {
    int e = april_read(OFF_SCAN_EPOCH);
    april_write(OFF_SCAN_EPOCH, e < 0 ? 0 : e + 1);
}

/* ── Message handler ─────────────────────────────────────── */
static void handle_msg(const char *buf) {
    char log[BUF_SIZE + 64];
    if (strncmp(buf, "APRIL|", 6) == 0) {
        snprintf(log, sizeof(log), "SPLINTER: %s", buf);
        klog(2, log);
        return;
    }
    if (strcmp(buf, "PING") == 0) {
        klog(2, "PING received");
        return;
    }
    snprintf(log, sizeof(log), "MSG: %s", buf);
    klog(2, log);
}

/* ── Client session ──────────────────────────────────────── */
static void run_client(int cfd) {
    char buf[BUF_SIZE];
    klog(1, "client connected");

    int flags = fcntl(cfd, F_GETFL, 0);
    fcntl(cfd, F_SETFL, flags | O_NONBLOCK);

    while (g_running) {
        int throttle = april_read(OFF_POLL_THROTTLE);
        int sleep_us = throttle > 0 ? throttle * 1000 : 100000;

        int n = read(cfd, buf, BUF_SIZE - 1);
        if (n > 0) {
            buf[n] = '\0';
            if (buf[n-1] == '\n') buf[n-1] = '\0';
            handle_msg(buf);
        } else if (n == 0) {
            klog(1, "client disconnected");
            break;
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            klog(1, "client read error");
            break;
        }

        bump_epoch();
        usleep(sleep_us);
    }
    close(cfd);
}

/* ── Server setup ────────────────────────────────────────── */
static int setup_server(void) {
    unlink(KRANG_SOCKET);
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, KRANG_SOCKET, sizeof(addr.sun_path) - 1);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd); return -1;
    }
    chmod(KRANG_SOCKET, 0700);
    listen(fd, 8);
    return fd;
}

/* ── Main ────────────────────────────────────────────────── */
int main(void) {
    signal(SIGTERM, handle_sig);
    signal(SIGINT,  handle_sig);
    signal(SIGPIPE, SIG_IGN);

    mkdir(BASE "/pipes",      0700);
    mkdir(BASE "/pipes/pids", 0700);
    mkdir(BASE "/Log_Cabin",  0755);

    if (april_read(OFF_SYSTEM_LOCK)) {
        klog(1, "SYSTEM_LOCK set — aborting");
        return 1;
    }

    write_pid();
    klog(1, "ONLINE");

    g_server_fd = setup_server();
    if (g_server_fd < 0) {
        klog(1, "FATAL: socket setup failed");
        unlink(PID_FILE);
        return 1;
    }

    klog(1, "listening on pipes/krang.sock");

    while (g_running) {
        int cfd = accept(g_server_fd, NULL, NULL);
        if (cfd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                bump_epoch();
                usleep(200000);
                continue;
            }
            if (!g_running) break;
            klog(1, "accept error — retrying");
            sleep(2);
            continue;
        }
        run_client(cfd);
    }

    if (g_server_fd >= 0) {
        close(g_server_fd);
        unlink(KRANG_SOCKET);
    }
    unlink(PID_FILE);
    klog(1, "OFFLINE — clean shutdown");
    return 0;
}
