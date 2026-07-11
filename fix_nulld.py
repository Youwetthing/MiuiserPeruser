#!/usr/bin/env python3
import sys, re

with open('src/daemon/nulld.c', 'r') as f:
    src = f.read()

# 1. Add errno.h after stdbool.h
src = src.replace('#include <stdbool.h>', '#include <stdbool.h>\n#include <errno.h>')

# 2. Inject APRIL shim after last #include
shim = '''
/* === APRIL emission shim === */
#include <sys/socket.h>
#include <sys/un.h>

#ifndef SPLINTER_SOCKET
#define SPLINTER_SOCKET "/data/local/tmp/MiuiserPeruser/pipes/splinter.sock"
#endif

#define IDLE_TX_THRESHOLD   8192
#define IDLE_TX_CRITICAL  65536

static int april_fd = -1;

static int april_connect(void) {
    if (april_fd >= 0) return april_fd;
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SPLINTER_SOCKET, sizeof(addr.sun_path)-1);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd); return -1;
    }
    april_fd = fd;
    return fd;
}

static void april_disconnect(void) {
    if (april_fd >= 0) { close(april_fd); april_fd = -1; }
}

static int april_emit(const char *type, const char *payload_fmt, ...) {
    char wire[2048], payload[1024];
    va_list ap;
    va_start(ap, payload_fmt);
    vsnprintf(payload, sizeof(payload), payload_fmt, ap);
    va_end(ap);
    int wlen = snprintf(wire, sizeof(wire), "APRIL|nulld|%s|%s\\n", type, payload);
    int fd = april_connect();
    if (fd < 0) return -1;
    ssize_t n = write(fd, wire, wlen);
    if (n < 0 && (errno == EPIPE || errno == ECONNRESET)) {
        april_disconnect();
        fd = april_connect();
        if (fd >= 0) n = write(fd, wire, wlen);
    }
    return (int)n;
}

/* === End APRIL shim === */
'''

last_include = src.rfind('#include')
eol = src.find('\n', last_include)
src = src[:eol+1] + shim + src[eol+1:]

# 3. Add screen state enum before first function/typedef
state_block = '''
typedef enum { SCREEN_ON, SCREEN_OFF, SCREEN_UNKNOWN } screen_state_t;

static screen_state_t g_screen = SCREEN_UNKNOWN;
static uint64_t g_idle_start_ms = 0;
static uint64_t g_baseline_tx = 0;
static uint64_t g_baseline_rx = 0;

static screen_state_t parse_screen(const char *dumpsys_out) {
    if (strstr(dumpsys_out, "mScreenState=ON")) return SCREEN_ON;
    if (strstr(dumpsys_out, "mScreenState=OFF")) return SCREEN_OFF;
    return SCREEN_UNKNOWN;
}

static void update_state(screen_state_t new_state, uint64_t now_ms,
                         uint64_t cur_tx, uint64_t cur_rx) {
    if (new_state == SCREEN_UNKNOWN) return;
    if (new_state == SCREEN_ON) {
        g_idle_start_ms = 0;
    } else if (new_state == SCREEN_OFF && g_screen != SCREEN_OFF) {
        g_idle_start_ms = now_ms;
        g_baseline_tx = cur_tx;
        g_baseline_rx = cur_rx;
    }
    g_screen = new_state;
}

'''

m = re.search(r'^[a-zA-Z_][a-zA-Z0-9_]*\s+\**[a-zA-Z_]', src, re.M)
if m:
    src = src[:m.start()] + state_block + src[m.start():]
else:
    src = state_block + src

# 4. Inject emission logic before tx_delta print
emit_block = '''
    /* APRIL emission: idle transmission anomaly detection */
    if (g_screen == SCREEN_OFF && idle_sec > 60 && tx_delta > 0) {
        int tcp_total = tcp4 + tcp6;
        if (tx_delta >= IDLE_TX_CRITICAL) {
            april_emit("idle_tx_critical",
                "{\\"idle_sec\\":%d,\\"tx_delta\\":%llu,\\"rx_delta\\":%llu,\\"tcp_established\\":%d}",
                idle_sec, (unsigned long long)tx_delta,
                (unsigned long long)rx_delta, tcp_total);
        } else if (tx_delta >= IDLE_TX_THRESHOLD) {
            april_emit("idle_tx",
                "{\\"idle_sec\\":%d,\\"tx_delta\\":%llu,\\"rx_delta\\":%llu,\\"tcp_established\\":%d}",
                idle_sec, (unsigned long long)tx_delta,
                (unsigned long long)rx_delta, tcp_total);
        }
    }
'''

tx_pos = src.find('tx_delta=')
if tx_pos >= 0:
    block_start = src.rfind('\n', 0, tx_pos) + 1
    src = src[:block_start] + emit_block + src[block_start:]

# 5. Fix unknown state reset
src = re.sub(
    r'if\s*\(\s*screen\s*==\s*SCREEN_UNKNOWN\s*\)\s*\{\s*[^}]*reset[^}]*\}',
    '/* unknown state retained — see update_state() */',
    src, flags=re.S | re.I
)

with open('src/daemon/nulld.c', 'w') as f:
    f.write(src)

print("Patched src/daemon/nulld.c")

