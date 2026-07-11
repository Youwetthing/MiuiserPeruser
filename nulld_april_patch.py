#!/usr/bin/env python3
"""
nulld_april_patch.py — Patch nulld.c to add APRIL bus emission

Usage:
    python3 nulld_april_patch.py src/core/nulld.c
    # or pipe:
    cat src/core/nulld.c | python3 nulld_april_patch.py - > nulld_patched.c
"""

import sys
import re

# --- The code blocks to inject ---

APRIL_HEADER = '''
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

SCREEN_STATE_ENUM = '''
typedef enum { SCREEN_ON, SCREEN_OFF, SCREEN_UNKNOWN } screen_state_t;
'''

STATE_MACHINE_PATCH = '''
/* Screen state machine: unknown retains previous state */
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
    if (new_state == SCREEN_UNKNOWN) return;  /* retain prior */
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

EMIT_LOGIC = '''
    /* APRIL emission: idle transmission anomaly detection */
    if (g_screen == SCREEN_OFF && idle_sec > 60 && tx_delta > 0) {
        if (tx_delta >= IDLE_TX_CRITICAL) {
            april_emit("idle_tx_critical",
                "{\\"idle_sec\\":%d,\\"tx_delta\\":%llu,\\"rx_delta\\":%llu,\\"tcp_established\\":%d}",
                idle_sec, (unsigned long long)tx_delta,
                (unsigned long long)rx_delta, tcp_count);
        } else if (tx_delta >= IDLE_TX_THRESHOLD) {
            april_emit("idle_tx",
                "{\\"idle_sec\\":%d,\\"tx_delta\\":%llu,\\"rx_delta\\":%llu,\\"tcp_established\\":%d}",
                idle_sec, (unsigned long long)tx_delta,
                (unsigned long long)rx_delta, tcp_count);
        }
    }
'''


def patch_nulld(source):
    out = source
    
    # 1. Inject APRIL header after last #include
    last_include = out.rfind('#include')
    if last_include >= 0:
        end_of_line = out.find('\n', last_include)
        out = out[:end_of_line+1] + APRIL_HEADER + out[end_of_line+1:]
    
    # 2. Add screen state enum near top (after includes, before first function/typedef)
    # Find a good insertion point: after includes, before first function
    first_func = re.search(r'^[a-zA-Z_][a-zA-Z0-9_]*\s+\**[a-zA-Z_]', out, re.M)
    if first_func:
        insert_at = first_func.start()
        out = out[:insert_at] + SCREEN_STATE_ENUM + '\n' + STATE_MACHINE_PATCH + '\n' + out[insert_at:]
    else:
        out = SCREEN_STATE_ENUM + '\n' + STATE_MACHINE_PATCH + '\n' + out
    
    # 3. Find the scan/print loop and inject emission logic before the print
    # Heuristic: look for "tx_delta" in a print context, inject before it
    tx_delta_print = out.find('tx_delta=')
    if tx_delta_print >= 0:
        # Find start of that block (previous newline)
        block_start = out.rfind('\n', 0, tx_delta_print) + 1
        out = out[:block_start] + EMIT_LOGIC + out[block_start:]
    
    # 4. Fix unknown state reset: replace any "unknown -> reset" logic
    # Pattern: if screen is unknown, treat as ON reset
    out = re.sub(
        r'if\s*\(\s*screen\s*==\s*SCREEN_UNKNOWN\s*\)\s*\{\s*[^}]*reset[^}]*\}',
        '/* unknown state retained — see update_state() */',
        out, flags=re.S | re.I
    )
    
    return out


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] != '-':
        with open(sys.argv[1], 'r') as f:
            source = f.read()
    else:
        source = sys.stdin.read()
    
    patched = patch_nulld(source)
    sys.stdout.write(patched)

