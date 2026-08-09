#!/usr/bin/env python3
"""
patch_nulld_splinterd.py

nulld.c currently has zero splinterd integration -- only gaveld_emit()
calls at all four signal sites (XIAOMI_IDLE_TELEMETRY,
IDLE_CONNECTION_SPIKE, IDLE_DATA_TRANSMISSION,
SUSPICIOUS_IDLE_TRANSMITTER), and no sys/socket.h or sys/un.h includes.

Adds splinterd_emit() (same pattern as rocksteadyd/every other fleet
daemon) and calls it alongside every existing gaveld_emit().

Anchors are deliberately short, low-indentation-dependent fragments
(not full whitespace-sensitive multi-line blocks) since the pasted
source had inconsistent terminal-wrap spacing.

Usage: run from ~/MiuiserPeruser
    python3 patch_nulld_splinterd.py
"""
import shutil
import sys

TARGET = "src/daemon/nulld.c"
BACKUP = TARGET + ".bak1"

EDITS = []

# --- Edit 1: add socket includes ----------------------------------------
EDITS.append((
    "Add sys/socket.h and sys/un.h includes",
    '#include "backend_exec.h"',
    '#include "backend_exec.h"\n#include <sys/socket.h>\n#include <sys/un.h>'
))

# --- Edit 2: add splinterd_emit() before check_adb_health() -------------
EDITS.append((
    "Add splinterd_emit() helper",
    'static int check_adb_health(void) {',
    '''static void splinterd_emit(const char *type, const char *payload)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return;
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SPLINTER_SOCKET, sizeof(addr.sun_path) - 1);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
        char buf[512];
        int n = snprintf(buf, sizeof(buf),
                         "APRIL|nulld|%s|%s\\n", type, payload);
        if (n > 0) write(fd, buf, (size_t)n);
    }
    close(fd);
}

static int check_adb_health(void) {'''
))

# --- Edit 3: wire XIAOMI_IDLE_TELEMETRY ----------------------------------
EDITS.append((
    "Wire splinterd_emit for XIAOMI_IDLE_TELEMETRY",
    '(double)delta, msg);',
    '(double)delta, msg);\n                    splinterd_emit("XIAOMI_IDLE_TELEMETRY", msg);'
))

# --- Edit 4: wire IDLE_CONNECTION_SPIKE ----------------------------------
EDITS.append((
    "Wire splinterd_emit for IDLE_CONNECTION_SPIKE",
    '(double)tcp_delta, msg);',
    '(double)tcp_delta, msg);\n                splinterd_emit("IDLE_CONNECTION_SPIKE", msg);'
))

# --- Edit 5: wire IDLE_DATA_TRANSMISSION ---------------------------------
EDITS.append((
    "Wire splinterd_emit for IDLE_DATA_TRANSMISSION",
    '(double)(idle_tx_delta/1024), msg);',
    '(double)(idle_tx_delta/1024), msg);\n                splinterd_emit("IDLE_DATA_TRANSMISSION", msg);'
))

# --- Edit 6: wire SUSPICIOUS_IDLE_TRANSMITTER ----------------------------
EDITS.append((
    "Wire splinterd_emit for SUSPICIOUS_IDLE_TRANSMITTER",
    '1.0, suspicious);',
    '1.0, suspicious);\n                    splinterd_emit("SUSPICIOUS_IDLE_TRANSMITTER", suspicious);'
))


def main():
    try:
        with open(TARGET, "r") as f:
            content = f.read()
    except FileNotFoundError:
        print(f"ABORT: {TARGET} not found -- run from ~/MiuiserPeruser")
        sys.exit(1)

    for label, old, new in EDITS:
        count = content.count(old)
        if count == 0:
            print(f"ABORT: anchor for '{label}' not found verbatim. No changes written.")
            sys.exit(1)
        if count > 1:
            print(f"ABORT: anchor for '{label}' matched {count} times, expected exactly 1. No changes written.")
            sys.exit(1)

    shutil.copy2(TARGET, BACKUP)
    print(f"Backed up {TARGET} -> {BACKUP}")

    for label, old, new in EDITS:
        content = content.replace(old, new)
        print(f"Applied: {label}")

    with open(TARGET, "w") as f:
        f.write(content)
    print(f"Patched {TARGET} ({len(EDITS)} edits)")

    if "splinterd_emit(" not in content:
        print("WARNING: no splinterd_emit call sites found post-patch -- check manually.")
    else:
        n = content.count("splinterd_emit(")
        print(f"Residual grep: {n} splinterd_emit() references (1 definition + 4 call sites expected = 5).")


if __name__ == "__main__":
    main()
