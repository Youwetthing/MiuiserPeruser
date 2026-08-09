#!/usr/bin/env python3
"""
patch_rocksteadyd_proc_batch_speedup.py

Root cause of the missing "Top processes" output + multi-minute gaps
between polls (observed live: 02:28:31 -> 02:30:47, 02:30:47 ->
02:32:59): the batched rish/adb_cli command from
patch_rocksteadyd_proc_bexec.py forks a separate `cat` per process
(`for p in /proc/[0-9]*/; do cat "${p}stat" ...; done`) -- with ~816
live processes on this device, that's ~816 sequential forks through
the adb_cli backend (rish itself isn't even in play here -- the
device's own log shows "[BEXEC] rish probe failed -- trying adb_cli"),
almost certainly hitting whatever timeout/latency ceiling bexec_n()
has before read_proc_procs() gets anything back.

Fix: let the shell glob expand /proc/[0-9]*/stat into ONE `cat`
invocation's argument list instead of looping. Single fork+exec
instead of ~816. Each /proc/PID/stat file already ends in its own
newline, so the explicit `echo` separator from the old command isn't
needed either -- dropping it, the parsing loop's blank-line skip stays
in place defensively but should rarely trigger now.

Usage:
    python3 patch_rocksteadyd_proc_batch_speedup.py ~/MiuiserPeruser/src/daemon/rocksteadyd.c
"""

import sys
import shutil

EDITS = []

EDITS.append((
    '    static const char *cmd =\n'
    '        "for p in /proc/[0-9]*/; do cat \\"${p}stat\\" 2>/dev/null; echo; done";',
    '    static const char *cmd = "cat /proc/[0-9]*/stat 2>/dev/null";'
))


def main():
    if len(sys.argv) != 2:
        print("Usage: python3 patch_rocksteadyd_proc_batch_speedup.py <path-to-rocksteadyd.c>")
        sys.exit(1)

    path = sys.argv[1]

    with open(path, "r", encoding="utf-8") as f:
        src = f.read()

    for idx, (old, new) in enumerate(EDITS, 1):
        n = src.count(old)
        if n != 1:
            print(f"ABORT: edit #{idx} matched {n} times (expected 1). No changes written.")
            print("---- old_str ----")
            print(old[:300])
            sys.exit(2)

    backup = path + ".bak5"
    shutil.copy2(path, backup)
    print(f"Backup written: {backup}")

    for old, new in EDITS:
        src = src.replace(old, new, 1)

    with open(path, "w", encoding="utf-8") as f:
        f.write(src)

    print(f"Patched: {path}")
    print(f"Edits applied: {len(EDITS)}")
    print("Next: cmake --build . --target rocksteadyd -j4")


if __name__ == "__main__":
    main()
