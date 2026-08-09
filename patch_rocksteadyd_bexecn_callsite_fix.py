#!/usr/bin/env python3
"""
patch_rocksteadyd_bexecn_callsite_fix.py

Fixes the bexec_n() call site from patch_rocksteadyd_proc_bexec.py.
Actual signature (from backend_exec.h, confirmed via build error):

    char *bexec_n(const char *cmd, size_t maxout);

-- it allocates and returns the buffer itself; it does not fill a
caller-provided one. Drops the now-unnecessary static g_proc_batch_buf
array, uses the returned pointer directly, and frees it at the end of
read_proc_procs() (this runs every poll -- leaking it would be a real
per-poll leak, not a one-time cost).

Run this once, after patch_rocksteadyd_proc_bexec.py has already been
applied (it has, per the build error) -- do not re-run the original
proc_bexec patch, it won't match anymore.

Usage:
    python3 patch_rocksteadyd_bexecn_callsite_fix.py ~/MiuiserPeruser/src/daemon/rocksteadyd.c
"""

import sys
import shutil

EDITS = []

# ---------------------------------------------------------------------
# 1. Drop the now-unnecessary static buffer, keep the size define
# ---------------------------------------------------------------------
EDITS.append((
    '#define PROC_BATCH_BUF_SIZE (512 * 1024)\n'
    'static char g_proc_batch_buf[PROC_BATCH_BUF_SIZE];',
    '#define PROC_BATCH_BUF_SIZE (512 * 1024)'
))

# ---------------------------------------------------------------------
# 2. Fix the call site: char *bexec_n(cmd, maxout), not an out-buffer
# ---------------------------------------------------------------------
EDITS.append((
    '    if (!bexec_n(cmd, g_proc_batch_buf, sizeof(g_proc_batch_buf))) {\n'
    '        return 0;\n'
    '    }\n'
    '\n'
    '    int count = 0;\n'
    '    char *line = g_proc_batch_buf;',
    '    char *proc_data = bexec_n(cmd, PROC_BATCH_BUF_SIZE);\n'
    '    if (!proc_data) {\n'
    '        return 0;\n'
    '    }\n'
    '\n'
    '    int count = 0;\n'
    '    char *line = proc_data;'
))

# ---------------------------------------------------------------------
# 3. Free the returned buffer before returning count
# ---------------------------------------------------------------------
EDITS.append((
    '        count++;\n'
    '        line = nl ? nl + 1 : NULL;\n'
    '    }\n'
    '\n'
    '    return count;\n'
    '}',
    '        count++;\n'
    '        line = nl ? nl + 1 : NULL;\n'
    '    }\n'
    '\n'
    '    free(proc_data);\n'
    '    return count;\n'
    '}'
))


def main():
    if len(sys.argv) != 2:
        print("Usage: python3 patch_rocksteadyd_bexecn_callsite_fix.py <path-to-rocksteadyd.c>")
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

    backup = path + ".bak4"
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
