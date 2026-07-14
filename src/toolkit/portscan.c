/*
 * portscan.c — minimal loopback TCP port scanner
 *
 * Purpose: replace `nmap` for the single job it's doing in the ADB
 * mDNS-port lookup — finding which port in a range is accepting
 * connections on 127.0.0.1. No raw sockets, no root, no external deps.
 *
 * Method: nonblocking connect() to every port in [start,end], poll()
 * them in chunks (default 512 at a time, to stay under the default
 * ulimit -n of 1024 without needing elevated privileges), check
 * SO_ERROR on writable fds. Loopback RTT is sub-millisecond, so a
 * 50ms poll timeout per chunk is generous, not a bottleneck.
 *
 * Usage: portscan <start_port> <end_port> [host] [-a]
 *   Prints the lowest open port and exits 0, or exits 1 with no
 *   output if nothing in the range is open.
 *   -a prints every open port found, ascending, one per line.
 *
 * Build: gcc src/toolkit/portscan.c -o bin/portscan
 *
 * Replaces:
 *   adb connect 127.0.0.1:$(nmap -p 30000-45000 127.0.0.1 | awk '/open/{print split($1,a,"/")?a[1]:""}')
 * with:
 *   PORT=$(bin/portscan 30000 45000 127.0.0.1)
 *   [ -n "$PORT" ] && adb connect 127.0.0.1:$PORT
 * (also fixes the original one-liner's bug: multiple open ports in
 * range made awk emit multiple lines, collapsing into one malformed
 * `adb connect` argument.)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define CHUNK 512       /* sockets in flight per batch */
#define TIMEOUT_MS 50   /* generous for loopback; real RTT is sub-ms */

static void scan_chunk(const char *ip, int start, int count,
                        int *open_ports, int *n_open) {
    int fds[CHUNK];
    struct pollfd pfds[CHUNK];
    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &addr.sin_addr);

    for (int i = 0; i < count; i++) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) { fds[i] = -1; pfds[i].fd = -1; continue; }
        fcntl(fd, F_SETFL, O_NONBLOCK);
        addr.sin_port = htons(start + i);
        connect(fd, (struct sockaddr *)&addr, sizeof(addr)); /* EINPROGRESS expected */
        fds[i] = fd;
        pfds[i].fd = fd;
        pfds[i].events = POLLOUT;
    }

    poll(pfds, count, TIMEOUT_MS);

    for (int i = 0; i < count; i++) {
        if (fds[i] < 0) continue;
        int err = 0;
        socklen_t len = sizeof(err);
        if ((pfds[i].revents & (POLLOUT | POLLERR)) &&
            getsockopt(fds[i], SOL_SOCKET, SO_ERROR, &err, &len) == 0 &&
            err == 0) {
            open_ports[(*n_open)++] = start + i;
        }
        close(fds[i]);
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <start_port> <end_port> [host] [-a]\n", argv[0]);
        return 2;
    }

    int start = atoi(argv[1]);
    int end = atoi(argv[2]);
    const char *host = "127.0.0.1";
    int show_all = 0;

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "-a") == 0) show_all = 1;
        else host = argv[i];
    }

    if (start < 1 || end < start || end > 65535) {
        fprintf(stderr, "invalid port range: %d-%d\n", start, end);
        return 2;
    }

    /* best-effort fd headroom bump; silently proceed if not permitted */
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) == 0) {
        rl.rlim_cur = rl.rlim_max;
        setrlimit(RLIMIT_NOFILE, &rl);
    }

    int total = end - start + 1;
    int *open_ports = malloc(sizeof(int) * total);
    int n_open = 0;

    for (int base = start; base <= end; base += CHUNK) {
        int count = (base + CHUNK - 1 <= end) ? CHUNK : (end - base + 1);
        scan_chunk(host, base, count, open_ports, &n_open);
    }

    if (n_open == 0) {
        free(open_ports);
        return 1;
    }

    /* ascending by construction, since chunks are scanned in order */
    if (show_all) {
        for (int i = 0; i < n_open; i++) printf("%d\n", open_ports[i]);
    } else {
        printf("%d\n", open_ports[0]);
    }

    free(open_ports);
    return 0;
}
