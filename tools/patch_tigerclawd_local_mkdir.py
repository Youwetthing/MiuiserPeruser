#!/usr/bin/env python3
import sys

PATH = "src/daemon/tigerclawd.c"

EDITS = []

EDITS.append(('''/* ── Baseline Management ───────────────────────────────────────────────── */''', '''static int local_mkdir_p(const char *path) {
    char tmp[512];
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\\0';
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) ? 0 : -1;
}

/* ── Baseline Management ───────────────────────────────────────────────── */'''))

EDITS.append(('''static void save_baseline(const tigerclaw_report_t *rpt) {
    char *mkdir_result = bexec("mkdir -p " MP_PIPES_DIR "/state "
                                 MP_PIPES_DIR "/daemon_results 2>/dev/null");
    free(mkdir_result);
    FILE *f = fopen(BASELINE_FILE, "w");
    if (!f) return;''', '''static void save_baseline(const tigerclaw_report_t *rpt) {
    if (local_mkdir_p(MP_PIPES_DIR "/state") != 0 ||
        local_mkdir_p(MP_PIPES_DIR "/daemon_results") != 0) {
        tlog("ERROR", "Failed to create baseline directories");
        return;
    }
    FILE *f = fopen(BASELINE_FILE, "w");
    if (!f) return;'''))

EDITS.append(('''static void write_json(const tigerclaw_report_t *rpt) {
    char *mkdir = bexec("mkdir -p " MP_PIPES_DIR "/daemon_results 2>/dev/null");
    free(mkdir);
    FILE *f = fopen(RESULTS_FILE, "w");
    if (!f) { tlog("ERROR", "Cannot write JSON results"); return; }''', '''static void write_json(const tigerclaw_report_t *rpt) {
    if (local_mkdir_p(MP_PIPES_DIR "/daemon_results") != 0) {
        tlog("ERROR", "Failed to create daemon_results directory");
        return;
    }
    FILE *f = fopen(RESULTS_FILE, "w");
    if (!f) {
        char errmsg[128];
        snprintf(errmsg, sizeof(errmsg), "Cannot write JSON results: %s", strerror(errno));
        tlog("ERROR", errmsg);
        return;
    }'''))

def main():
    with open(PATH, "r") as f:
        content = f.read()

    for i, (old, new) in enumerate(EDITS):
        count = content.count(old)
        if count != 1:
            print(f"ABORT at edit {i}: expected exactly 1 match, found {count}", file=sys.stderr)
            print(f"--- OLD snippet ---\n{old[:200]}", file=sys.stderr)
            sys.exit(1)
        content = content.replace(old, new)

    with open(PATH, "w") as f:
        f.write(content)

    print(f"OK: applied {len(EDITS)} edits to {PATH}")

if __name__ == "__main__":
    main()
