// sysport.c — MiuiserPeruser System Port Server
// Rootless, ADB-less backend privilege engine.
// Listens on 127.0.0.1:8765 and exposes a tiny, auditable command set:
//
//   READ_SYS  <path>      # read from /sys
//   READ_PROC <path>      # read from /proc
//   GETPROP   <key>       # getprop key
//   PING                   # health check
//
// Responses are single-line, newline-terminated:
//   OK <data...>
//   ERR <reason>
//
// This is intentionally minimal and conservative.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

#define SYS_PORT_LISTEN_ADDR "127.0.0.1"
#define SYS_PORT_LISTEN_PORT 8765
#define BUF_SIZE 4096
#define MAX_LINE 2048

// -----------------------------
// Utility: trim whitespace
// -----------------------------
static char *trim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    if (!*s) return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
    return s;
}

// -----------------------------
// Utility: safe prefix check
// -----------------------------
static int has_prefix(const char *s, const char *pfx) {
    size_t ls = strlen(s), lp = strlen(pfx);
    return ls >= lp && strncmp(s, pfx, lp) == 0;
}

// -----------------------------
// Read a single line from fd
// -----------------------------
static ssize_t read_line(int fd, char *buf, size_t sz) {
    size_t i = 0;
    while (i + 1 < sz) {
        char c;
        ssize_t n = read(fd, &c, 1);
        if (n <= 0) {
            if (i == 0) return n;
            break;
        }
        if (c == '\n') break;
        buf[i++] = c;
    }
    buf[i] = '\0';
    return (ssize_t)i;
}

// -----------------------------
// Send a line to fd
// -----------------------------
static void send_line(int fd, const char *line) {
    if (!line) return;
    size_t len = strlen(line);
    write(fd, line, len);
    write(fd, "\n", 1);
}

// -----------------------------
// Handle READ_SYS
// -----------------------------
static void handle_read_sys(int fd, const char *path) {
    if (!path || !*path) {
        send_line(fd, "ERR missing_path");
        return;
    }
    if (!has_prefix(path, "/sys/")) {
        send_line(fd, "ERR invalid_sys_path");
        return;
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        send_line(fd, "ERR open_failed");
        return;
    }

    char buf[BUF_SIZE];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);

    buf[n] = '\0';
    // Strip newlines to keep response single-line
    for (size_t i = 0; i < n; i++) {
        if (buf[i] == '\n' || buf[i] == '\r') buf[i] = ' ';
    }

    char out[BUF_SIZE + 4];
    snprintf(out, sizeof(out), "OK %s", buf);
    send_line(fd, out);
}

// -----------------------------
// Handle READ_PROC
// -----------------------------
static void handle_read_proc(int fd, const char *path) {
    if (!path || !*path) {
        send_line(fd, "ERR missing_path");
        return;
    }
    if (!has_prefix(path, "/proc/")) {
        send_line(fd, "ERR invalid_proc_path");
        return;
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        send_line(fd, "ERR open_failed");
        return;
    }

    char buf[BUF_SIZE];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);

    buf[n] = '\0';
    for (size_t i = 0; i < n; i++) {
        if (buf[i] == '\n' || buf[i] == '\r') buf[i] = ' ';
    }

    char out[BUF_SIZE + 4];
    snprintf(out, sizeof(out), "OK %s", buf);
    send_line(fd, out);
}

// -----------------------------
// Handle GETPROP
// -----------------------------
static void handle_getprop(int fd, const char *key) {
    if (!key || !*key) {
        send_line(fd, "ERR missing_key");
        return;
    }

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "getprop '%s' 2>/dev/null", key);

    FILE *p = popen(cmd, "r");
    if (!p) {
        send_line(fd, "ERR getprop_failed");
        return;
    }

    char buf[BUF_SIZE];
    if (!fgets(buf, sizeof(buf), p)) {
        pclose(p);
        send_line(fd, "ERR getprop_empty");
        return;
    }
    pclose(p);

    buf[strcspn(buf, "\r\n")] = '\0';

    char out[BUF_SIZE + 4];
    snprintf(out, sizeof(out), "OK %s", buf);
    send_line(fd, out);
}

// -----------------------------
// Handle a single client
// -----------------------------
static void handle_client(int cfd) {
    char line[MAX_LINE];

    while (1) {
        ssize_t n = read_line(cfd, line, sizeof(line));
        if (n <= 0) break;

        char *t = trim(line);
        if (!*t) continue;

        char cmd[32];
        char arg[1024];

        cmd[0] = '\0';
        arg[0] = '\0';

        int count = sscanf(t, "%31s %1023[^\n]", cmd, arg);
        if (count <= 0) {
            send_line(cfd, "ERR bad_command");
            continue;
        }

        if (strcmp(cmd, "PING") == 0) {
            send_line(cfd, "OK PONG");
        } else if (strcmp(cmd, "READ_SYS") == 0) {
            handle_read_sys(cfd, count >= 2 ? trim(arg) : NULL);
        } else if (strcmp(cmd, "READ_PROC") == 0) {
            handle_read_proc(cfd, count >= 2 ? trim(arg) : NULL);
        } else if (strcmp(cmd, "GETPROP") == 0) {
            handle_getprop(cfd, count >= 2 ? trim(arg) : NULL);
        } else {
            send_line(cfd, "ERR unknown_command");
        }
    }

    close(cfd);
}

// -----------------------------
// Main server loop
// -----------------------------
int main(void) {
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SYS_PORT_LISTEN_PORT);
    inet_pton(AF_INET, SYS_PORT_LISTEN_ADDR, &addr.sin_addr);

    if (bind(sfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sfd);
        return 1;
    }

    if (listen(sfd, 4) < 0) {
        perror("listen");
        close(sfd);
        return 1;
    }

    fprintf(stderr, "[sysport] listening on %s:%d\n",
            SYS_PORT_LISTEN_ADDR, SYS_PORT_LISTEN_PORT);

    while (1) {
        int cfd = accept(sfd, NULL, NULL);
        if (cfd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            break;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            close(cfd);
            continue;
        }

        if (pid == 0) {
            // child
            close(sfd);
            handle_client(cfd);
            _exit(0);
        }

        // parent
        close(cfd);
    }

    close(sfd);
    return 0;
}
// ------------------------------------------------------------
// Handle DUMPSYS <service>
// Safe version: executes "dumpsys <service>" and returns output.
// ------------------------------------------------------------
static void handle_dumpsys(int fd, const char *service) {
    if (!service || !*service) {
        send_line(fd, "ERR missing_service");
        return;
    }

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "dumpsys %s 2>/dev/null", service);

    FILE *p = popen(cmd, "r");
    if (!p) {
        send_line(fd, "ERR dumpsys_failed");
        return;
    }

    char buf[BUF_SIZE];
    size_t n = fread(buf, 1, sizeof(buf) - 1, p);
    pclose(p);

    if (n == 0) {
        send_line(fd, "ERR dumpsys_empty");
        return;
    }

    buf[n] = '\0';

    // Flatten newlines to keep response single-line
    for (size_t i = 0; i < n; i++) {
        if (buf[i] == '\n' || buf[i] == '\r') buf[i] = ' ';
    }

    char out[BUF_SIZE + 4];
    snprintf(out, sizeof(out), "OK %s", buf);
    send_line(fd, out);
}
        else if (strcmp(cmd, "DUMPSYS") == 0) {
            handle_dumpsys(cfd, count >= 2 ? trim(arg) : NULL);
        }
