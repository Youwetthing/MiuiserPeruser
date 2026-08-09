#!/usr/bin/env python3
"""
patch_rocksteadyd_procstat_bexec.py

Fixes: read_proc_stat() called fopen("/proc/stat") directly as plain
Termux. Under this device's hidepid=invisible mount that line can
come back blank/stale (same visibility class already fixed in
read_proc_procs()), which pins total_delta at 0 in compute_proc_pcts()
and silently zeroes every process's cpu_pct -- explaining the
"Top processes:" section being empty across all 8 scans in the
2026-08-09 live log even after timing was fixed. The display loop's
`if (procs[i].cpu_pct <= 0) break;` then exits on the very first
(highest-ranked) entry.

Fix: route /proc/stat through bexec_n(), same pattern as
read_proc_procs(), parsing the "cpu " line out of the batched output.

Usage: run from ~/MiuiserPeruser
    python3 patch_rocksteadyd_procstat_bexec.py
"""
import shutil
import sys

TARGET = "src/daemon/rocksteadyd.c"
BACKUP = TARGET + ".bak6"

OLD = '''static bool read_proc_stat(cpu_stat_t *st)
{
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return false;
    char line[256];
    bool ok = false;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "cpu ", 4) == 0) {
            sscanf(line + 4, "%llu %llu %llu %llu %llu %llu %llu",
                   &st->user, &st->nice, &st->system, &st->idle,
                   &st->iowait, &st->irq, &st->softirq);
            ok = true;
            break;
        }
    }
    fclose(f);
    return ok;
}'''

NEW = '''#define PROC_STAT_BUF_SIZE (4 * 1024)

static bool read_proc_stat(cpu_stat_t *st)
{
    /* Same hidepid-visibility class as read_proc_procs() -- plain
     * Termux fopen() on /proc/stat can return blank/stale data under
     * this device's hidepid=invisible mount. Route through bexec_n()
     * for consistency with the rest of the /proc read path. */
    char *data = bexec_n("cat /proc/stat 2>/dev/null", PROC_STAT_BUF_SIZE);
    if (!data) return false;

    bool ok = false;
    char *line = data;
    while (line && *line) {
        char *nl = strchr(line, '\\n');
        if (nl) *nl = '\\0';

        if (strncmp(line, "cpu ", 4) == 0) {
            sscanf(line + 4, "%llu %llu %llu %llu %llu %llu %llu",
                   &st->user, &st->nice, &st->system, &st->idle,
                   &st->iowait, &st->irq, &st->softirq);
            ok = true;
            break;
        }

        line = nl ? nl + 1 : NULL;
    }

    free(data);
    return ok;
}'''

def main():
    try:
        with open(TARGET, "r") as f:
            content = f.read()
    except FileNotFoundError:
        print(f"ABORT: {TARGET} not found -- run from ~/MiuiserPeruser")
        sys.exit(1)

    count = content.count(OLD)
    if count == 0:
        print("ABORT: anchor text for read_proc_stat() not found verbatim.")
        print("File may already be patched, or has diverged from expected state.")
        sys.exit(1)
    if count > 1:
        print(f"ABORT: anchor text matched {count} times, expected exactly 1.")
        sys.exit(1)

    shutil.copy2(TARGET, BACKUP)
    print(f"Backed up {TARGET} -> {BACKUP}")

    patched = content.replace(OLD, NEW)
    with open(TARGET, "w") as f:
        f.write(patched)
    print(f"Patched {TARGET} (1 edit)")

    if "fopen(\"/proc/stat\"" in patched:
        print("WARNING: residual fopen(\"/proc/stat\" reference still present -- check manually.")
    else:
        print("Residual grep clean: no direct fopen(\"/proc/stat\") calls remain.")

if __name__ == "__main__":
    main()
