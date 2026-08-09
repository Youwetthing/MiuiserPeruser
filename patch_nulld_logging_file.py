#!/usr/bin/env python3
"""
patch_nulld_logging_file.py

tlog() already fires consistently at every call site (this part of
nulld was already fleet-standard) -- what's missing is the optional
file-output leg every other daemon has (ROCKSTEADYD_LOG_PATH,
FUGITOIDD_LOG_PATH, etc). Adds NULLD_LOG_PATH support: stderr output
is unchanged (still fires with no env var, per the fleet's
live-rolling-output-without-flags convention), file output is added
alongside when the env var is set.

Usage: run from ~/MiuiserPeruser
    python3 patch_nulld_logging_file.py
"""
import shutil
import sys

TARGET = "src/daemon/nulld.c"
BACKUP = TARGET + ".bak3"

EDITS = []

# --- Edit 1: add g_nulld_log_fp + nulldlog_init() before tlog() ---------
EDITS.append((
    "Add nulldlog_init() and g_nulld_log_fp",
    'static void tlog(const char *lvl, const char *msg) {',
    '''static FILE *g_nulld_log_fp = NULL;

static void nulldlog_init(void) {
    const char *path = getenv("NULLD_LOG_PATH");
    if (path && path[0]) {
        g_nulld_log_fp = fopen(path, "a");
    }
}

static void tlog(const char *lvl, const char *msg) {'''
))

# --- Edit 2: write to file alongside stderr in tlog() --------------------
EDITS.append((
    "Add file-write leg to tlog()",
    'fprintf(stderr, "[%s][NULLD/%s] %s\\n", ts, lvl, msg);',
    '''fprintf(stderr, "[%s][NULLD/%s] %s\\n", ts, lvl, msg);
    if (g_nulld_log_fp) {
        fprintf(g_nulld_log_fp, "[%s][NULLD/%s] %s\\n", ts, lvl, msg);
        fflush(g_nulld_log_fp);
    }'''
))

# --- Edit 3: call nulldlog_init() in main() alongside bexec_init() ------
EDITS.append((
    "Call nulldlog_init() in main()",
    'bexec_init();',
    'bexec_init();\n    nulldlog_init();'
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

    if "nulldlog_init()" not in content:
        print("WARNING: nulldlog_init() not found post-patch -- check manually.")
    else:
        print("Residual grep clean: nulldlog_init() wired in.")


if __name__ == "__main__":
    main()
