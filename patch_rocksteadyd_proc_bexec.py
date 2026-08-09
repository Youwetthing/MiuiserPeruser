#!/usr/bin/env python3
"""
patch_rocksteadyd_proc_bexec.py

Fixes the confirmed hidepid visibility bug in rocksteadyd.c:
read_proc_procs() called opendir("/proc") directly as plain Termux,
which under this device's hidepid=invisible mount sees ~6 processes
instead of the real ~816 (same bug class already confirmed and fixed
in ratkingd -- Termux lacks AID_READPROC/gid 3009, rish has it).

Fix: one batched rish command (`for p in /proc/[0-9]*/; do cat
"${p}stat" 2>/dev/null; echo; done`) pulls every /proc/PID/stat line
in a single bexec_n() call instead of per-PID fopen(), same batching
lesson as tigerclawd/shredderd/the planned ratkingd rewrite. Per-line
parsing logic (name extraction, utime/stime fields) is UNCHANGED from
the original -- only the source of lines changes, from opendir()+
per-PID fopen() to one batched string.

Also bumps MAX_PROCS 256 -> 1024. This wasn't reachable before (only
~6 processes were ever visible), but fixing visibility exposes a real
truncation bug: at 816 live processes, the old cap of 256 would have
silently dropped ~560 of them, in glob order (roughly ascending PID --
i.e. it would have dropped newer/foreground processes first, the ones
most likely to be legitimate top-CPU consumers). Both proc_t arrays
(the poll()-local `procs[MAX_PROCS]` and the global `g_prev_procs`)
scale with the same #define, so this is a one-line fix at the top of
the file, not a signature change.

IMPORTANT -- unverified assumption: this patch calls bexec_n(cmd, buf,
bufsize) matching the signature established by metalheadd's fix
("routes through bexec_n() with explicit 128KB buffer"). That call
shape was inferred from prior session notes, not read directly from
backend_exec.h in this session. If bexec_n()'s actual signature
differs (return type, arg order, or a separate error-out param), fix
the one call site in read_proc_procs() -- the rest of the patch does
not depend on it.

Buffer sized generously (512 KiB) for headroom above the observed 816
processes; if the device's process count grows substantially further,
the buffer may need to grow again -- not currently truncation-checked.

Run independently of the other two rocksteadyd patches -- no overlap
with the cluster or logging changes, order doesn't matter relative to
those.

Usage:
    python3 patch_rocksteadyd_proc_bexec.py ~/MiuiserPeruser/src/daemon/rocksteadyd.c
"""

import sys
import shutil

EDITS = []

# ---------------------------------------------------------------------
# 1. Bump MAX_PROCS -- was fine at ~6 visible processes, silently
#    truncates at ~816 real ones once visibility is fixed below.
# ---------------------------------------------------------------------
EDITS.append((
    '#define MAX_PROCS         256',
    '#define MAX_PROCS         1024  /* was 256 -- too low once real\n'
    '                                   /proc visibility is restored,\n'
    '                                   see proc_bexec patch */'
))

# ---------------------------------------------------------------------
# 2. read_proc_procs(): opendir()/per-PID fopen() -> single batched
#    bexec_n() call. Per-line parsing logic unchanged.
# ---------------------------------------------------------------------
EDITS.append((
    'static int read_proc_procs(proc_t *procs, int max_procs)\n'
    '{\n'
    '    DIR *d = opendir("/proc");\n'
    '    if (!d) return 0;\n'
    '\n'
    '    int count = 0;\n'
    '    struct dirent *ent;\n'
    '\n'
    '    while ((ent = readdir(d)) != NULL && count < max_procs) {\n'
    '        /* Only numeric dirs */\n'
    '        if (ent->d_name[0] < \'1\' || ent->d_name[0] > \'9\') continue;\n'
    '\n'
    '        int pid = atoi(ent->d_name);\n'
    '        if (pid <= 0) continue;\n'
    '\n'
    '        char path[64];\n'
    '        snprintf(path, sizeof(path), "/proc/%d/stat", pid);\n'
    '        FILE *f = fopen(path, "r");\n'
    '        if (!f) continue;\n'
    '\n'
    '        char line[512];\n'
    '        if (!fgets(line, sizeof(line), f)) { fclose(f); continue; }\n'
    '        fclose(f);\n'
    '\n'
    '        /* Format: pid (name) state ppid ... utime stime ... */\n'
    '        proc_t *p = &procs[count];\n'
    '        p->pid = pid;\n'
    '        p->cpu_time = 0;\n'
    '        p->cpu_pct  = 0;\n'
    '        p->name[0]  = \'\\0\';\n'
    '\n'
    '        /* Extract name from (name) */\n'
    '        char *nb = strchr(line, \'(\');\n'
    '        char *ne = strrchr(line, \')\');\n'
    '        if (nb && ne && ne > nb) {\n'
    '            size_t len = (size_t)(ne - nb - 1);\n'
    '            if (len >= sizeof(p->name)) len = sizeof(p->name) - 1;\n'
    '            strncpy(p->name, nb + 1, len);\n'
    '            p->name[len] = \'\\0\';\n'
    '        }\n'
    '\n'
    '        /* Fields after \')\': state ppid pgrp sid ... utime(14) stime(15) */\n'
    '        if (ne) {\n'
    '            unsigned long utime = 0, stime = 0;\n'
    '            int fields = sscanf(ne + 2,\n'
    '                "%*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u "\n'
    '                "%lu %lu", &utime, &stime);\n'
    '            if (fields == 2)\n'
    '                p->cpu_time = (unsigned long long)utime + (unsigned long long)stime;\n'
    '        }\n'
    '\n'
    '        count++;\n'
    '    }\n'
    '    closedir(d);\n'
    '    return count;\n'
    '}',
    '/* Direct opendir("/proc") as plain Termux only sees processes visible\n'
    ' * under this device\'s hidepid=invisible mount (~6 of ~816 confirmed\n'
    ' * live) -- Termux lacks AID_READPROC/gid 3009, rish has it. Route\n'
    ' * through bexec_n() with a single batched rish call instead of\n'
    ' * per-PID fopen(); parsing logic below is unchanged from before. */\n'
    '\n'
    '#define PROC_BATCH_BUF_SIZE (512 * 1024)\n'
    'static char g_proc_batch_buf[PROC_BATCH_BUF_SIZE];\n'
    '\n'
    'static int read_proc_procs(proc_t *procs, int max_procs)\n'
    '{\n'
    '    static const char *cmd =\n'
    '        "for p in /proc/[0-9]*/; do cat \\"${p}stat\\" 2>/dev/null; echo; done";\n'
    '\n'
    '    if (!bexec_n(cmd, g_proc_batch_buf, sizeof(g_proc_batch_buf))) {\n'
    '        return 0;\n'
    '    }\n'
    '\n'
    '    int count = 0;\n'
    '    char *line = g_proc_batch_buf;\n'
    '\n'
    '    while (line && *line && count < max_procs) {\n'
    '        char *nl = strchr(line, \'\\n\');\n'
    '        if (nl) *nl = \'\\0\';\n'
    '\n'
    '        if (line[0] == \'\\0\') { line = nl ? nl + 1 : NULL; continue; }\n'
    '\n'
    '        int pid = atoi(line);\n'
    '        if (pid <= 0) { line = nl ? nl + 1 : NULL; continue; }\n'
    '\n'
    '        /* Format: pid (name) state ppid ... utime stime ... */\n'
    '        proc_t *p = &procs[count];\n'
    '        p->pid = pid;\n'
    '        p->cpu_time = 0;\n'
    '        p->cpu_pct  = 0;\n'
    '        p->name[0]  = \'\\0\';\n'
    '\n'
    '        /* Extract name from (name) */\n'
    '        char *nb = strchr(line, \'(\');\n'
    '        char *ne = strrchr(line, \')\');\n'
    '        if (nb && ne && ne > nb) {\n'
    '            size_t len = (size_t)(ne - nb - 1);\n'
    '            if (len >= sizeof(p->name)) len = sizeof(p->name) - 1;\n'
    '            strncpy(p->name, nb + 1, len);\n'
    '            p->name[len] = \'\\0\';\n'
    '        }\n'
    '\n'
    '        /* Fields after \')\': state ppid pgrp sid ... utime(14) stime(15) */\n'
    '        if (ne) {\n'
    '            unsigned long utime = 0, stime = 0;\n'
    '            int fields = sscanf(ne + 2,\n'
    '                "%*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u "\n'
    '                "%lu %lu", &utime, &stime);\n'
    '            if (fields == 2)\n'
    '                p->cpu_time = (unsigned long long)utime + (unsigned long long)stime;\n'
    '        }\n'
    '\n'
    '        count++;\n'
    '        line = nl ? nl + 1 : NULL;\n'
    '    }\n'
    '\n'
    '    return count;\n'
    '}'
))


def main():
    if len(sys.argv) != 2:
        print("Usage: python3 patch_rocksteadyd_proc_bexec.py <path-to-rocksteadyd.c>")
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

    backup = path + ".bak3"
    shutil.copy2(path, backup)
    print(f"Backup written: {backup}")

    for old, new in EDITS:
        src = src.replace(old, new, 1)

    with open(path, "w", encoding="utf-8") as f:
        f.write(src)

    print(f"Patched: {path}")
    print(f"Edits applied: {len(EDITS)}")
    print("Next: cmake --build . --target rocksteadyd -j4")
    print("If build fails on bexec_n() signature mismatch, that's the")
    print("one unverified assumption flagged in this script's docstring.")


if __name__ == "__main__":
    main()
