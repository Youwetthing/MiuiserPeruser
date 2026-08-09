#!/usr/bin/env python3
"""
patch_rocksteadyd_cluster_agnostic.py

Generalizes rocksteadyd.c's enumerate_cpus()/poll() from a hardcoded
2-cluster (efficiency/performance) threshold split to N-cluster
detection grouped by distinct cpuinfo_max_freq.

Usage:
    python3 patch_rocksteadyd_cluster_agnostic.py ~/MiuiserPeruser/src/daemon/rocksteadyd.c

Verbatim-match-or-abort: every old_str must match exactly once or the
script aborts before writing anything. Makes a .bak copy first.
"""

import sys
import shutil

EDITS = []

# ---------------------------------------------------------------------
# 1. Add MAX_CLUSTERS define
# ---------------------------------------------------------------------
EDITS.append((
    '#define TOP_N             5     /* top processes to display */',
    '#define TOP_N             5     /* top processes to display */\n'
    '#define MAX_CLUSTERS      4     /* generous ceiling for tri/quad-gear SoCs */'
))

# ---------------------------------------------------------------------
# 2. Replace enumerate_cpus() signature + cluster-detection tail
# ---------------------------------------------------------------------
EDITS.append((
    'static int enumerate_cpus(cpu_core_t *cores, int max_cores)\n'
    '{\n'
    '    int count = 0;\n'
    '    char path[256];\n'
    '    long max_seen = 0;\n',
    'static int enumerate_cpus(cpu_core_t *cores, int max_cores, int *out_nclusters)\n'
    '{\n'
    '    int count = 0;\n'
    '    char path[256];\n'
))

EDITS.append((
    '        c->throttled = (c->max_khz > 0 &&\n'
    '                        (float)c->cur_khz < (float)c->max_khz * THROTTLE_RATIO);\n'
    '        c->at_max = (c->max_khz > 0 && c->cur_khz >= c->max_khz);\n'
    '\n'
    '        /* Track global max for cluster detection */\n'
    '        if (c->max_khz > max_seen) max_seen = c->max_khz;\n'
    '\n'
    '        c->cluster = -1; /* assigned below */\n'
    '        count++;\n'
    '    }\n'
    '\n'
    '    /* Cluster detection: cores with lower max_khz = efficiency */\n'
    '    if (count > 1 && max_seen > 0) {\n'
    '        long threshold = (long)((float)max_seen * 0.85f);\n'
    '        for (int i = 0; i < count; i++) {\n'
    '            cores[i].cluster = (cores[i].max_khz < threshold) ? 0 : 1;\n'
    '        }\n'
    '    }\n'
    '\n'
    '    return count;\n'
    '}',
    '        c->throttled = (c->max_khz > 0 &&\n'
    '                        (float)c->cur_khz < (float)c->max_khz * THROTTLE_RATIO);\n'
    '        c->at_max = (c->max_khz > 0 && c->cur_khz >= c->max_khz);\n'
    '\n'
    '        c->cluster = -1; /* assigned below */\n'
    '        count++;\n'
    '    }\n'
    '\n'
    '    /* Cluster detection: group cores by distinct cpuinfo_max_freq,\n'
    '     * sorted ascending. Cluster index = rank (0 = lowest freq).\n'
    '     * Works for any cluster count, not just a fixed eff/perf split. */\n'
    '    long distinct_freqs[MAX_CLUSTERS];\n'
    '    int  n_distinct = 0;\n'
    '\n'
    '    for (int i = 0; i < count; i++) {\n'
    '        long f = cores[i].max_khz;\n'
    '        if (f <= 0) continue;\n'
    '\n'
    '        bool found = false;\n'
    '        for (int j = 0; j < n_distinct; j++) {\n'
    '            if (distinct_freqs[j] == f) { found = true; break; }\n'
    '        }\n'
    '        if (!found && n_distinct < MAX_CLUSTERS) {\n'
    '            distinct_freqs[n_distinct++] = f;\n'
    '        }\n'
    '    }\n'
    '\n'
    '    for (int i = 1; i < n_distinct; i++) {\n'
    '        long key = distinct_freqs[i];\n'
    '        int j = i - 1;\n'
    '        while (j >= 0 && distinct_freqs[j] > key) {\n'
    '            distinct_freqs[j + 1] = distinct_freqs[j];\n'
    '            j--;\n'
    '        }\n'
    '        distinct_freqs[j + 1] = key;\n'
    '    }\n'
    '\n'
    '    for (int i = 0; i < count; i++) {\n'
    '        if (cores[i].max_khz <= 0) continue;\n'
    '        for (int j = 0; j < n_distinct; j++) {\n'
    '            if (cores[i].max_khz == distinct_freqs[j]) {\n'
    '                cores[i].cluster = j;\n'
    '                break;\n'
    '            }\n'
    '        }\n'
    '        /* max_khz didn\'t match any tracked bucket (only possible if\n'
    '         * n_distinct hit MAX_CLUSTERS) -- leave cluster at -1, safe */\n'
    '    }\n'
    '\n'
    '    if (out_nclusters) *out_nclusters = n_distinct;\n'
    '    return count;\n'
    '}'
))

# ---------------------------------------------------------------------
# 3. poll(): enumerate_cpus() call site
# ---------------------------------------------------------------------
EDITS.append((
    '    /* ?? CPU cores ?????????????????????????????????????????????????????? */\n'
    '    cpu_core_t cores[MAX_CORES];\n'
    '    int ncores = enumerate_cpus(cores, MAX_CORES);\n'
    '\n'
    '    int throttled_count  = 0;\n'
    '    int maxed_count      = 0;\n'
    '    int perf_gov_count   = 0;\n'
    '    long eff_cur = 0, eff_max = 0, perf_cur = 0, perf_max = 0;\n'
    '    int  eff_n   = 0, perf_n  = 0;',
    '    /* ?? CPU cores ?????????????????????????????????????????????????????? */\n'
    '    cpu_core_t cores[MAX_CORES];\n'
    '    int nclusters = 0;\n'
    '    int ncores = enumerate_cpus(cores, MAX_CORES, &nclusters);\n'
    '\n'
    '    int throttled_count  = 0;\n'
    '    int maxed_count      = 0;\n'
    '    int perf_gov_count   = 0;\n'
    '    long clus_cur[MAX_CLUSTERS] = {0};\n'
    '    long clus_max[MAX_CLUSTERS] = {0};\n'
    '    int  clus_n[MAX_CLUSTERS]   = {0};'
))

# ---------------------------------------------------------------------
# 4. poll(): per-core display label + accumulation
# ---------------------------------------------------------------------
EDITS.append((
    '        const char *cl = c->cluster == 0 ? "[E]"\n'
    '                       : c->cluster == 1 ? "[P]" : "   ";\n'
    '\n'
    '        printf("[ROCKY]  cpu%-2d%s %-7ld  %-7ld  %-12s  %s\\n",',
    '        char cl[8];\n'
    '        if (c->cluster >= 0)\n'
    '            snprintf(cl, sizeof(cl), "[c%d]", c->cluster);\n'
    '        else\n'
    '            snprintf(cl, sizeof(cl), "    ");\n'
    '\n'
    '        printf("[ROCKY]  cpu%-2d%s %-7ld  %-7ld  %-12s  %s\\n",'
))

EDITS.append((
    '        /* Cluster utilisation */\n'
    '        if (c->cluster == 0) {\n'
    '            eff_cur += c->cur_khz; eff_max += c->max_khz; eff_n++;\n'
    '        } else if (c->cluster == 1) {\n'
    '            perf_cur += c->cur_khz; perf_max += c->max_khz; perf_n++;\n'
    '        }\n'
    '    }',
    '        if (c->cluster >= 0 && c->cluster < MAX_CLUSTERS) {\n'
    '            clus_cur[c->cluster] += c->cur_khz;\n'
    '            clus_max[c->cluster] += c->max_khz;\n'
    '            clus_n[c->cluster]++;\n'
    '        }\n'
    '    }'
))

# ---------------------------------------------------------------------
# 5. poll(): cluster balance block (2-cluster -> N-cluster worst-pair)
# ---------------------------------------------------------------------
EDITS.append((
    '    /* Cluster balance */\n'
    '    float eff_util  = (eff_n  > 0 && eff_max  > 0) ? 100.0f * (float)eff_cur  / (float)eff_max  : 0;\n'
    '    float perf_util = (perf_n > 0 && perf_max > 0) ? 100.0f * (float)perf_cur / (float)perf_max : 0;\n'
    '\n'
    '    if (eff_n > 0 && perf_n > 0) {\n'
    '        printf("[ROCKY]  Efficiency cluster: %.0f%%  Performance cluster: %.0f%%\\n",\n'
    '               eff_util, perf_util);\n'
    '\n'
    '        /* Efficiency running harder than performance = scheduler imbalance */\n'
    '        if (eff_util > perf_util + 20.0f) {\n'
    '            char ctx[80];\n'
    '            snprintf(ctx, sizeof(ctx),\n'
    '                     "eff=%.0f%% perf=%.0f%% delta=%.0f%%",\n'
    '                     eff_util, perf_util, eff_util - perf_util);\n'
    '            gaveld_emit(DAEMON_NAME, "CPU_CLUSTER_IMBALANCE", eff_util - perf_util, ctx);\n'
    '            splinterd_emit("CPU_CLUSTER_IMBALANCE", ctx);\n'
    '            score -= 12;\n'
    '            sigs++;\n'
    '            printf("[ROCKY]  !  Cluster imbalance -- efficiency overloaded\\n");\n'
    '        }\n'
    '    }',
    '    /* Cluster balance -- generalized to N clusters */\n'
    '    if (nclusters > 1) {\n'
    '        float clus_util[MAX_CLUSTERS] = {0};\n'
    '        printf("[ROCKY]  Cluster util:");\n'
    '        for (int i = 0; i < nclusters; i++) {\n'
    '            clus_util[i] = (clus_n[i] > 0 && clus_max[i] > 0)\n'
    '                         ? 100.0f * (float)clus_cur[i] / (float)clus_max[i]\n'
    '                         : 0.0f;\n'
    '            printf("  c%d=%.0f%%", i, clus_util[i]);\n'
    '        }\n'
    '        printf("\\n");\n'
    '\n'
    '        /* A lower-frequency cluster running hotter than a higher one\n'
    '         * it should be offloading to is a scheduler placement problem,\n'
    '         * regardless of how many clusters exist. Report the worst\n'
    '         * offending (lower, higher) pair, one firing per poll. */\n'
    '        int   worst_lo = -1, worst_hi = -1;\n'
    '        float worst_delta = 0.0f;\n'
    '\n'
    '        for (int lo = 0; lo < nclusters; lo++) {\n'
    '            if (clus_n[lo] == 0) continue;\n'
    '            for (int hi = lo + 1; hi < nclusters; hi++) {\n'
    '                if (clus_n[hi] == 0) continue;\n'
    '                float delta = clus_util[lo] - clus_util[hi];\n'
    '                if (delta > worst_delta) {\n'
    '                    worst_delta = delta;\n'
    '                    worst_lo = lo;\n'
    '                    worst_hi = hi;\n'
    '                }\n'
    '            }\n'
    '        }\n'
    '\n'
    '        if (worst_lo >= 0 && worst_delta > 20.0f) {\n'
    '            char ctx[96];\n'
    '            snprintf(ctx, sizeof(ctx),\n'
    '                     "c%d=%.0f%% c%d=%.0f%% delta=%.0f%%",\n'
    '                     worst_lo, clus_util[worst_lo],\n'
    '                     worst_hi, clus_util[worst_hi], worst_delta);\n'
    '            gaveld_emit(DAEMON_NAME, "CPU_CLUSTER_IMBALANCE", worst_delta, ctx);\n'
    '            splinterd_emit("CPU_CLUSTER_IMBALANCE", ctx);\n'
    '            score -= 12;\n'
    '            sigs++;\n'
    '            printf("[ROCKY]  !  Cluster imbalance -- c%d overloaded vs c%d\\n",\n'
    '                   worst_lo, worst_hi);\n'
    '        }\n'
    '    }'
))


def main():
    if len(sys.argv) != 2:
        print("Usage: python3 patch_rocksteadyd_cluster_agnostic.py <path-to-rocksteadyd.c>")
        sys.exit(1)

    path = sys.argv[1]

    with open(path, "r", encoding="utf-8") as f:
        src = f.read()

    # Verify every edit matches exactly once before touching anything
    for idx, (old, new) in enumerate(EDITS, 1):
        n = src.count(old)
        if n != 1:
            print(f"ABORT: edit #{idx} matched {n} times (expected 1). No changes written.")
            print("---- old_str ----")
            print(old[:300])
            sys.exit(2)

    backup = path + ".bak"
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
