#!/usr/bin/env python3
"""
patch_rocksteadyd_logging_config.py

1. Adds rockylog()/rockylog_init() unified logger (stderr + optional
   ROCKSTEADYD_LOG_PATH file dest), matching fleet convention
   (fugitoidlog/leatherlog/metalheadlog/ratkinglog/burnedlog/rzlog).
   Converts startup/exit control-flow printfs + the 6 warning/signal
   printfs to rockylog(). Per-poll dashboard (scan banner, core table,
   cluster util line, top-processes list, score summary) intentionally
   LEFT AS RAW PRINTF -- matches ratkingd/leatherheadd/fugitoidd
   convention of not routing high-frequency display output through
   the file logger.

2. Migrates config_get_int() off popen()+jq onto fopen/strstr, closing
   the fleet popen()-audit gap for this daemon. NOTE: this is a naive
   single-level JSON key search bounded to the daemon's own section by
   its closing brace -- matches the existing simple-parser convention
   used elsewhere in the fleet, not a general JSON parser. Flag if
   STATE_FILE's shape is more nested than that assumption.

Run this AFTER patch_rocksteadyd_cluster_agnostic.py -- edit #6 below
matches the cluster-imbalance printf in its POST-cluster-patch form.

Usage:
    python3 patch_rocksteadyd_logging_config.py ~/MiuiserPeruser/src/daemon/rocksteadyd.c
"""

import sys
import shutil

EDITS = []

# ---------------------------------------------------------------------
# 1. #include <stdarg.h> for the variadic logger
# ---------------------------------------------------------------------
EDITS.append((
    '#include <sys/socket.h>\n'
    '#include <sys/un.h>\n'
    '#include <stdbool.h>',
    '#include <sys/socket.h>\n'
    '#include <sys/un.h>\n'
    '#include <stdbool.h>\n'
    '#include <stdarg.h>'
))

# ---------------------------------------------------------------------
# 2. Insert rockylog infra before the Config section
# ---------------------------------------------------------------------
EDITS.append((
    'static cpu_stat_t g_prev_stat = {0};\n'
    'static proc_t     g_prev_procs[MAX_PROCS];\n'
    'static int        g_prev_nprocs = 0;\n'
    'static bool       g_first = true;\n'
    '\n'
    '/* ?? Config ????????????????????????????????????????????????????????????? */',
    'static cpu_stat_t g_prev_stat = {0};\n'
    'static proc_t     g_prev_procs[MAX_PROCS];\n'
    'static int        g_prev_nprocs = 0;\n'
    'static bool       g_first = true;\n'
    '\n'
    '/* ?? Logging ???????????????????????????????????????????????????????????? */\n'
    '\n'
    'static FILE *g_rocky_log_fp = NULL;\n'
    '\n'
    'static void rockylog_init(void)\n'
    '{\n'
    '    const char *path = getenv("ROCKSTEADYD_LOG_PATH");\n'
    '    if (path && path[0]) {\n'
    '        g_rocky_log_fp = fopen(path, "a");\n'
    '    }\n'
    '}\n'
    '\n'
    'static void rockylog(const char *level, const char *fmt, ...)\n'
    '{\n'
    '    time_t t = time(NULL);\n'
    '    char ts[32];\n'
    '    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", localtime(&t));\n'
    '\n'
    '    va_list ap1, ap2;\n'
    '    va_start(ap1, fmt);\n'
    '    va_copy(ap2, ap1);\n'
    '\n'
    '    fprintf(stderr, "[%s][ROCKSTEADYD/%s] ", ts, level);\n'
    '    vfprintf(stderr, fmt, ap1);\n'
    '    fprintf(stderr, "\\n");\n'
    '    va_end(ap1);\n'
    '\n'
    '    if (g_rocky_log_fp) {\n'
    '        fprintf(g_rocky_log_fp, "[%s][ROCKSTEADYD/%s] ", ts, level);\n'
    '        vfprintf(g_rocky_log_fp, fmt, ap2);\n'
    '        fprintf(g_rocky_log_fp, "\\n");\n'
    '        fflush(g_rocky_log_fp);\n'
    '    }\n'
    '    va_end(ap2);\n'
    '}\n'
    '\n'
    '/* ?? Config ????????????????????????????????????????????????????????????? */'
))

# ---------------------------------------------------------------------
# 3. config_get_int(): popen()+jq -> fopen/strstr
# ---------------------------------------------------------------------
EDITS.append((
    'static int config_get_int(const char *key, int def)\n'
    '{\n'
    '    char cmd[512];\n'
    '    snprintf(cmd, sizeof(cmd),\n'
    '             "jq -r \'.%s.%s // %d\' %s 2>/dev/null",\n'
    '             DAEMON_NAME, key, def, STATE_FILE);\n'
    '    FILE *f = popen(cmd, "r");\n'
    '    if (!f) return def;\n'
    '    char buf[32] = {0};\n'
    '    int val = def;\n'
    '    if (fgets(buf, sizeof(buf), f) && buf[0] != \'n\')\n'
    '        val = atoi(buf);\n'
    '    pclose(f);\n'
    '    return val;\n'
    '}',
    'static int config_get_int(const char *key, int def)\n'
    '{\n'
    '    FILE *f = fopen(STATE_FILE, "r");\n'
    '    if (!f) return def;\n'
    '\n'
    '    char buf[4096];\n'
    '    size_t n = fread(buf, 1, sizeof(buf) - 1, f);\n'
    '    fclose(f);\n'
    '    buf[n] = \'\\0\';\n'
    '\n'
    '    /* Naive single-level lookup: find "rocksteadyd" section, then the\n'
    '     * key within it, bounded by the section\'s closing brace so a\n'
    '     * same-named key in a sibling daemon\'s config can\'t bleed in.\n'
    '     * Matches the existing fleet fopen/strstr convention -- not a\n'
    '     * general JSON parser. */\n'
    '    char section_key[64];\n'
    '    snprintf(section_key, sizeof(section_key), "\\"%s\\"", DAEMON_NAME);\n'
    '    char *section = strstr(buf, section_key);\n'
    '    if (!section) return def;\n'
    '\n'
    '    char *section_end = strchr(section, \'}\');\n'
    '\n'
    '    char field_key[64];\n'
    '    snprintf(field_key, sizeof(field_key), "\\"%s\\"", key);\n'
    '    char *field = strstr(section, field_key);\n'
    '    if (!field || (section_end && field > section_end)) return def;\n'
    '\n'
    '    field = strchr(field, \':\');\n'
    '    if (!field) return def;\n'
    '    field++;\n'
    '    while (*field == \' \') field++;\n'
    '\n'
    '    if (strncmp(field, "null", 4) == 0) return def;\n'
    '\n'
    '    return atoi(field);\n'
    '}'
))

# ---------------------------------------------------------------------
# 4. main(): rockylog_init() call + startup/exit printf -> rockylog()
# ---------------------------------------------------------------------
EDITS.append((
    'int main(void)\n'
    '{\n'
    '    bexec_init();\n'
    '\n'
    '    if (!is_enabled()) {\n'
    '        printf("[ROCKY] disabled via syndicatectl -- exiting\\n");\n'
    '        return 0;\n'
    '    }\n'
    '\n'
    '    printf("[ROCKY] CPU Load, Frequency & Cluster Balance Daemon: ONLINE\\n");\n'
    '    printf("[ROCKY] HOG threshold: %d%%  Critical: %d%%\\n",\n'
    '           HOG_PCT, HOG_CRITICAL_PCT);',
    'int main(void)\n'
    '{\n'
    '    bexec_init();\n'
    '    rockylog_init();\n'
    '\n'
    '    if (!is_enabled()) {\n'
    '        rockylog("INFO", "disabled via syndicatectl -- exiting");\n'
    '        return 0;\n'
    '    }\n'
    '\n'
    '    rockylog("INFO", "CPU Load, Frequency & Cluster Balance Daemon: ONLINE");\n'
    '    rockylog("INFO", "HOG threshold: %d%%  Critical: %d%%",\n'
    '             HOG_PCT, HOG_CRITICAL_PCT);'
))

EDITS.append((
    '        if (!is_enabled()) {\n'
    '            printf("[ROCKY] disabled -- stopping\\n");\n'
    '            break;\n'
    '        }',
    '        if (!is_enabled()) {\n'
    '            rockylog("INFO", "disabled -- stopping");\n'
    '            break;\n'
    '        }'
))

EDITS.append((
    '        if (max_scans > 0 && scan_num >= max_scans) {\n'
    '            printf("[ROCKY] reached scan_count=%d -- exiting\\n", max_scans);\n'
    '            break;\n'
    '        }',
    '        if (max_scans > 0 && scan_num >= max_scans) {\n'
    '            rockylog("INFO", "reached scan_count=%d -- exiting", max_scans);\n'
    '            break;\n'
    '        }'
))

EDITS.append((
    '        printf("[ROCKY] Next scan in %ds\\n", interval);\n'
    '        sleep(interval);\n'
    '    }\n'
    '\n'
    '    return 0;\n'
    '}',
    '        printf("[ROCKY] Next scan in %ds\\n", interval);\n'
    '        sleep(interval);\n'
    '    }\n'
    '\n'
    '    if (g_rocky_log_fp) fclose(g_rocky_log_fp);\n'
    '    return 0;\n'
    '}'
))

# ---------------------------------------------------------------------
# 5. poll(): 6 warning/signal printfs -> rockylog("WARN", ...)
#    (dashboard/table/banner printfs are intentionally left untouched)
# ---------------------------------------------------------------------

# 5a. Cluster imbalance -- matches the POST cluster-agnostic-patch text
EDITS.append((
    '            printf("[ROCKY]  !  Cluster imbalance -- c%d overloaded vs c%d\\n",\n'
    '                   worst_lo, worst_hi);',
    '            rockylog("WARN", "Cluster imbalance -- c%d overloaded vs c%d",\n'
    '                     worst_lo, worst_hi);'
))

# 5b. Throttling
EDITS.append((
    '        printf("[ROCKY]  !  %d/%d cores throttled\\n", throttled_count, ncores);',
    '        rockylog("WARN", "%d/%d cores throttled", throttled_count, ncores);'
))

# 5c. All cores maxed
EDITS.append((
    '        printf("[ROCKY]  !  All cores at max frequency\\n");',
    '        rockylog("WARN", "All cores at max frequency");'
))

# 5d. Performance governor
EDITS.append((
    '        printf("[ROCKY]  !  %d core(s) locked to performance governor\\n",\n'
    '               perf_gov_count);',
    '        rockylog("WARN", "%d core(s) locked to performance governor",\n'
    '                 perf_gov_count);'
))

# 5e. Critical hog
EDITS.append((
    '                printf("[ROCKY]    !! CRITICAL HOG: %s\\n", procs[i].name);',
    '                rockylog("WARN", "CRITICAL HOG: %s", procs[i].name);'
))

# 5f. Hog
EDITS.append((
    '                printf("[ROCKY]    !  HOG: %s\\n", procs[i].name);',
    '                rockylog("WARN", "HOG: %s", procs[i].name);'
))


def main():
    if len(sys.argv) != 2:
        print("Usage: python3 patch_rocksteadyd_logging_config.py <path-to-rocksteadyd.c>")
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

    backup = path + ".bak2"
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
