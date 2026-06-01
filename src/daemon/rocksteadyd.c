/*
 * rocksteadyd.c -- CPU Load, Frequency & Cluster Balance Daemon
 *
 * Redmi 15C ? HyperOS OS2.0 ? MediaTek ? 8-core (cpu0-3 efficiency,
 * cpu4-7 performance) ? Android 15
 *
 * Every poll:
 *   - Read cur/max/min freq + governor per core from cpufreq sysfs
 *   - Compute per-cluster utilisation (efficiency vs performance)
 *   - Read /proc/stat for system-wide CPU delta across polls
 *   - Walk /proc/[pid]/stat for top CPU consumers
 *   - Detect throttling, governor anomalies, cluster imbalance
 *   - Score CPU posture 0-100
 *   - Emit gaveld signals and write results
 *
 * Adaptive:
 *   - Discovers core count dynamically
 *   - Detects cluster split by max_freq difference
 *   - Works on any number of cores/clusters
 *
 * Gaveld signals:
 *   CPU_HOG, CPU_HOG_CRITICAL, CPU_THROTTLING, CPU_CLUSTER_IMBALANCE,
 *   GOVERNOR_PERFORMANCE, ALL_CORES_MAXED
 *
 * Runtime config: enabled, interval (default 15), scan_count
 */

#include "ipc_globals.h"
#include "gaveld_emit.h"
#include "backend_exec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdbool.h>

#define DAEMON_NAME       "rocksteadyd"
#define DEFAULT_INTERVAL  15
#define MAX_CORES         16
#define MAX_PROCS         256
#define HOG_PCT           40    /* single process CPU% = hog */
#define HOG_CRITICAL_PCT  70    /* single process CPU% = critical */
#define THROTTLE_RATIO    0.75f /* cur < max * ratio = throttled */
#define TOP_N             5     /* top processes to display */

#ifndef MP_BASE_DIR
#define MP_BASE_DIR "/data/data/com.termux/files/home/MiuiserPeruser"
#endif

#define STATE_FILE   MP_BASE_DIR "/Registry/daemon_state.json"
#define RESULTS_DIR  MP_BASE_DIR "/Registry/daemon_results"
#define RESULTS_FILE RESULTS_DIR "/" DAEMON_NAME ".json"

/* ?? CPU core record ???????????????????????????????????????????????????? */

typedef struct {
    int  core;
    long cur_khz;
    long max_khz;
    long min_khz;
    char governor[32];
    bool throttled;
    bool at_max;
    int  cluster;   /* 0 = efficiency, 1 = performance, -1 = unknown */
} cpu_core_t;

/* ?? /proc/stat snapshot ???????????????????????????????????????????????? */

typedef struct {
    unsigned long long user, nice, system, idle, iowait, irq, softirq;
} cpu_stat_t;

/* ?? Process CPU record ????????????????????????????????????????????????? */

typedef struct {
    int   pid;
    char  name[32];
    unsigned long long cpu_time;   /* utime + stime */
    int   cpu_pct;                 /* computed delta */
} proc_t;

/* ?? State across polls ????????????????????????????????????????????????? */

static cpu_stat_t g_prev_stat = {0};
static proc_t     g_prev_procs[MAX_PROCS];
static int        g_prev_nprocs = 0;
static bool       g_first = true;

/* ?? Config ????????????????????????????????????????????????????????????? */

static int config_get_int(const char *key, int def)
{
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "jq -r '.%s.%s // %d' %s 2>/dev/null",
             DAEMON_NAME, key, def, STATE_FILE);
    FILE *f = popen(cmd, "r");
    if (!f) return def;
    char buf[32] = {0};
    int val = def;
    if (fgets(buf, sizeof(buf), f) && buf[0] != 'n')
        val = atoi(buf);
    pclose(f);
    return val;
}

static int is_enabled(void)    { return config_get_int("enabled",    1); }
static int get_interval(void)  { return config_get_int("interval",   DEFAULT_INTERVAL); }
static int get_max_scans(void) { return config_get_int("scan_count", 0); }

/* ?? Splinterd emit ????????????????????????????????????????????????????? */

static void splinterd_emit(const char *type, const char *payload)
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
                         "APRIL|" DAEMON_NAME "|%s|%s\n", type, payload);
        if (n > 0) write(fd, buf, (size_t)n);
    }
    close(fd);
}

/* ?? sysfs helpers ?????????????????????????????????????????????????????? */

static long read_long_sysfs(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    long v = -1;
    fscanf(f, "%ld", &v);
    fclose(f);
    return v;
}

/* ?? Enumerate CPU cores ??????????????????????????????????????????????? */

static int enumerate_cpus(cpu_core_t *cores, int max_cores)
{
    int count = 0;
    char path[256];
    long max_seen = 0;

    for (int i = 0; i < max_cores; i++) {
        snprintf(path, sizeof(path),
                 "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", i);
        long cur = read_long_sysfs(path);
        if (cur < 0) break;

        cpu_core_t *c = &cores[count];
        c->core = i;
        c->cur_khz = cur;

        snprintf(path, sizeof(path),
                 "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", i);
        c->max_khz = read_long_sysfs(path);

        snprintf(path, sizeof(path),
                 "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_min_freq", i);
        c->min_khz = read_long_sysfs(path);

        snprintf(path, sizeof(path),
                 "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor", i);
        FILE *gf = fopen(path, "r");
        if (gf) {
            fgets(c->governor, sizeof(c->governor), gf);
            c->governor[strcspn(c->governor, "\n")] = '\0';
            fclose(gf);
        } else {
            strncpy(c->governor, "unknown", sizeof(c->governor) - 1);
        }

        c->throttled = (c->max_khz > 0 &&
                        (float)c->cur_khz < (float)c->max_khz * THROTTLE_RATIO);
        c->at_max = (c->max_khz > 0 && c->cur_khz >= c->max_khz);

        /* Track global max for cluster detection */
        if (c->max_khz > max_seen) max_seen = c->max_khz;

        c->cluster = -1; /* assigned below */
        count++;
    }

    /* Cluster detection: cores with lower max_khz = efficiency */
    if (count > 1 && max_seen > 0) {
        long threshold = (long)((float)max_seen * 0.85f);
        for (int i = 0; i < count; i++) {
            cores[i].cluster = (cores[i].max_khz < threshold) ? 0 : 1;
        }
    }

    return count;
}

/* ?? /proc/stat CPU total ??????????????????????????????????????????????? */

static bool read_proc_stat(cpu_stat_t *st)
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
}

static int compute_cpu_pct(const cpu_stat_t *prev, const cpu_stat_t *cur)
{
    unsigned long long prev_total = prev->user + prev->nice + prev->system +
                                    prev->idle + prev->iowait + prev->irq +
                                    prev->softirq;
    unsigned long long cur_total  = cur->user  + cur->nice  + cur->system  +
                                    cur->idle  + cur->iowait + cur->irq   +
                                    cur->softirq;
    unsigned long long dtotal = cur_total - prev_total;
    if (dtotal == 0) return 0;
    unsigned long long didle = cur->idle - prev->idle;
    return (int)(100ULL * (dtotal - didle) / dtotal);
}

/* ?? /proc/[pid]/stat reader ???????????????????????????????????????????? */

static int read_proc_procs(proc_t *procs, int max_procs)
{
    DIR *d = opendir("/proc");
    if (!d) return 0;

    int count = 0;
    struct dirent *ent;

    while ((ent = readdir(d)) != NULL && count < max_procs) {
        /* Only numeric dirs */
        if (ent->d_name[0] < '1' || ent->d_name[0] > '9') continue;

        int pid = atoi(ent->d_name);
        if (pid <= 0) continue;

        char path[64];
        snprintf(path, sizeof(path), "/proc/%d/stat", pid);
        FILE *f = fopen(path, "r");
        if (!f) continue;

        char line[512];
        if (!fgets(line, sizeof(line), f)) { fclose(f); continue; }
        fclose(f);

        /* Format: pid (name) state ppid ... utime stime ... */
        proc_t *p = &procs[count];
        p->pid = pid;
        p->cpu_time = 0;
        p->cpu_pct  = 0;
        p->name[0]  = '\0';

        /* Extract name from (name) */
        char *nb = strchr(line, '(');
        char *ne = strrchr(line, ')');
        if (nb && ne && ne > nb) {
            size_t len = (size_t)(ne - nb - 1);
            if (len >= sizeof(p->name)) len = sizeof(p->name) - 1;
            strncpy(p->name, nb + 1, len);
            p->name[len] = '\0';
        }

        /* Fields after ')': state ppid pgrp sid ... utime(14) stime(15) */
        if (ne) {
            unsigned long utime = 0, stime = 0;
            int fields = sscanf(ne + 2,
                "%*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u "
                "%lu %lu", &utime, &stime);
            if (fields == 2)
                p->cpu_time = (unsigned long long)utime + (unsigned long long)stime;
        }

        count++;
    }
    closedir(d);
    return count;
}

/* ?? Compute per-process CPU delta ?????????????????????????????????????? */

static void compute_proc_pcts(proc_t *procs, int nprocs,
                               unsigned long long total_delta)
{
    if (total_delta == 0 || g_first) return;

    for (int i = 0; i < nprocs; i++) {
        /* Find matching pid in prev */
        for (int j = 0; j < g_prev_nprocs; j++) {
            if (g_prev_procs[j].pid == procs[i].pid) {
                unsigned long long dt = procs[i].cpu_time - g_prev_procs[j].cpu_time;
                procs[i].cpu_pct = (int)(100ULL * dt / total_delta);
                break;
            }
        }
    }
}

/* Simple insertion sort top-N by cpu_pct */
static void sort_top_n(proc_t *procs, int nprocs)
{
    for (int i = 1; i < nprocs; i++) {
        proc_t key = procs[i];
        int j = i - 1;
        while (j >= 0 && procs[j].cpu_pct < key.cpu_pct) {
            procs[j + 1] = procs[j];
            j--;
        }
        procs[j + 1] = key;
    }
}

/* ?? Results writer ????????????????????????????????????????????????????? */

static void write_results(int score, int scan_num, int sigs,
                           int sys_pct, int ncores, int throttled)
{
    FILE *f = fopen(RESULTS_FILE, "w");
    if (!f) return;

    time_t t = time(NULL);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", localtime(&t));

    const char *grade = score >= 85 ? "HEALTHY"
                      : score >= 65 ? "BUSY"
                      : score >= 45 ? "STRESSED"
                      : "OVERLOADED";

    fprintf(f,
        "{\n"
        "  \"daemon\": \"" DAEMON_NAME "\",\n"
        "  \"timestamp\": \"%s\",\n"
        "  \"scan_number\": %d,\n"
        "  \"signals_fired\": %d,\n"
        "  \"cpu_score\": %d,\n"
        "  \"grade\": \"%s\",\n"
        "  \"system_cpu_pct\": %d,\n"
        "  \"total_cores\": %d,\n"
        "  \"throttled_cores\": %d\n"
        "}\n",
        ts, scan_num, sigs, score, grade,
        sys_pct, ncores, throttled);

    fclose(f);
}

/* ?? Main poll ?????????????????????????????????????????????????????????? */

static void poll(int scan_num)
{
    char ts[32];
    time_t t = time(NULL);
    strftime(ts, sizeof(ts), "%H:%M:%S", localtime(&t));

    printf("\n[ROCKY] ?? CPU Scan #%d  %s ??????????????????????????????\n",
           scan_num, ts);

    int score = 100;
    int sigs  = 0;

    /* ?? System CPU % ??????????????????????????????????????????????????? */
    cpu_stat_t cur_stat = {0};
    int sys_pct = 0;
    if (read_proc_stat(&cur_stat)) {
        if (!g_first)
            sys_pct = compute_cpu_pct(&g_prev_stat, &cur_stat);
        g_prev_stat = cur_stat;
    }
    printf("[ROCKY]  System CPU  : %d%%\n", sys_pct);

    /* ?? CPU cores ?????????????????????????????????????????????????????? */
    cpu_core_t cores[MAX_CORES];
    int ncores = enumerate_cpus(cores, MAX_CORES);

    int throttled_count  = 0;
    int maxed_count      = 0;
    int perf_gov_count   = 0;
    long eff_cur = 0, eff_max = 0, perf_cur = 0, perf_max = 0;
    int  eff_n   = 0, perf_n  = 0;

    printf("[ROCKY]  %-5s  %-7s  %-7s  %-12s  %s\n",
           "Core", "CurMHz", "MaxMHz", "Governor", "State");
    printf("[ROCKY]  ??????????????????????????????????????????????????\n");

    for (int i = 0; i < ncores; i++) {
        cpu_core_t *c = &cores[i];
        const char *state = c->throttled ? "THROTTLED"
                          : c->at_max    ? "MAX"
                          : "OK";
        const char *cl = c->cluster == 0 ? "[E]"
                       : c->cluster == 1 ? "[P]" : "   ";

        printf("[ROCKY]  cpu%-2d%s %-7ld  %-7ld  %-12s  %s\n",
               c->core, cl,
               c->cur_khz / 1000, c->max_khz / 1000,
               c->governor, state);

        if (c->throttled) throttled_count++;
        if (c->at_max)    maxed_count++;
        if (strcmp(c->governor, "performance") == 0) perf_gov_count++;

        /* Cluster utilisation */
        if (c->cluster == 0) {
            eff_cur += c->cur_khz; eff_max += c->max_khz; eff_n++;
        } else if (c->cluster == 1) {
            perf_cur += c->cur_khz; perf_max += c->max_khz; perf_n++;
        }
    }

    /* Cluster balance */
    float eff_util  = (eff_n  > 0 && eff_max  > 0) ? 100.0f * (float)eff_cur  / (float)eff_max  : 0;
    float perf_util = (perf_n > 0 && perf_max > 0) ? 100.0f * (float)perf_cur / (float)perf_max : 0;

    if (eff_n > 0 && perf_n > 0) {
        printf("[ROCKY]  Efficiency cluster: %.0f%%  Performance cluster: %.0f%%\n",
               eff_util, perf_util);

        /* Efficiency running harder than performance = scheduler imbalance */
        if (eff_util > perf_util + 20.0f) {
            char ctx[80];
            snprintf(ctx, sizeof(ctx),
                     "eff=%.0f%% perf=%.0f%% delta=%.0f%%",
                     eff_util, perf_util, eff_util - perf_util);
            gaveld_emit(DAEMON_NAME, "CPU_CLUSTER_IMBALANCE", eff_util - perf_util, ctx);
            splinterd_emit("CPU_CLUSTER_IMBALANCE", ctx);
            score -= 12;
            sigs++;
            printf("[ROCKY]  !  Cluster imbalance -- efficiency overloaded\n");
        }
    }

    /* Throttling */
    if (throttled_count > 0) {
        char ctx[48];
        snprintf(ctx, sizeof(ctx), "throttled=%d/%d", throttled_count, ncores);
        gaveld_emit(DAEMON_NAME, "CPU_THROTTLING", (float)throttled_count, ctx);
        splinterd_emit("CPU_THROTTLING", ctx);
        score -= 8 * throttled_count;
        sigs++;
        printf("[ROCKY]  !  %d/%d cores throttled\n", throttled_count, ncores);
    }

    /* All cores maxed */
    if (ncores > 0 && maxed_count == ncores) {
        char ctx[32];
        snprintf(ctx, sizeof(ctx), "cores=%d", ncores);
        gaveld_emit(DAEMON_NAME, "ALL_CORES_MAXED", (float)ncores, ctx);
        splinterd_emit("ALL_CORES_MAXED", ctx);
        score -= 15;
        sigs++;
        printf("[ROCKY]  !  All cores at max frequency\n");
    }

    /* Performance governor */
    if (perf_gov_count > 0) {
        char ctx[32];
        snprintf(ctx, sizeof(ctx), "cores=%d", perf_gov_count);
        gaveld_emit(DAEMON_NAME, "GOVERNOR_PERFORMANCE", (float)perf_gov_count, ctx);
        splinterd_emit("GOVERNOR_PERFORMANCE", ctx);
        score -= 8;
        sigs++;
        printf("[ROCKY]  !  %d core(s) locked to performance governor\n",
               perf_gov_count);
    }

    /* ?? Per-process CPU ???????????????????????????????????????????????? */
    proc_t procs[MAX_PROCS];
    int nprocs = read_proc_procs(procs, MAX_PROCS);

    /* Compute total CPU delta for normalisation */
    unsigned long long prev_total = g_prev_stat.user + g_prev_stat.nice +
                                    g_prev_stat.system + g_prev_stat.idle +
                                    g_prev_stat.iowait + g_prev_stat.irq +
                                    g_prev_stat.softirq;
    unsigned long long cur_total  = cur_stat.user + cur_stat.nice +
                                    cur_stat.system + cur_stat.idle +
                                    cur_stat.iowait + cur_stat.irq +
                                    cur_stat.softirq;
    unsigned long long total_delta = cur_total > prev_total ?
                                     cur_total - prev_total : 0;

    compute_proc_pcts(procs, nprocs, total_delta);
    sort_top_n(procs, nprocs);

    int top = nprocs < TOP_N ? nprocs : TOP_N;
    if (!g_first && top > 0) {
        printf("[ROCKY]  Top processes:\n");
        for (int i = 0; i < top; i++) {
            if (procs[i].cpu_pct <= 0) break;
            printf("[ROCKY]    %3d%%  pid=%-6d  %s\n",
                   procs[i].cpu_pct, procs[i].pid, procs[i].name);

            if (procs[i].cpu_pct >= HOG_CRITICAL_PCT) {
                char ctx[80];
                snprintf(ctx, sizeof(ctx), "pid=%d name=%.24s pct=%d",
                         procs[i].pid, procs[i].name, procs[i].cpu_pct);
                gaveld_emit(DAEMON_NAME, "CPU_HOG_CRITICAL",
                            (float)procs[i].cpu_pct, ctx);
                splinterd_emit("CPU_HOG_CRITICAL", ctx);
                score -= 25;
                sigs++;
                printf("[ROCKY]    !! CRITICAL HOG: %s\n", procs[i].name);
            } else if (procs[i].cpu_pct >= HOG_PCT) {
                char ctx[80];
                snprintf(ctx, sizeof(ctx), "pid=%d name=%.24s pct=%d",
                         procs[i].pid, procs[i].name, procs[i].cpu_pct);
                gaveld_emit(DAEMON_NAME, "CPU_HOG",
                            (float)procs[i].cpu_pct, ctx);
                splinterd_emit("CPU_HOG", ctx);
                score -= 15;
                sigs++;
                printf("[ROCKY]    !  HOG: %s\n", procs[i].name);
            }
        }
    }

    /* Save state for next poll */
    if (nprocs > MAX_PROCS) nprocs = MAX_PROCS;
    memcpy(g_prev_procs, procs, (size_t)nprocs * sizeof(proc_t));
    g_prev_nprocs = nprocs;
    g_first = false;

    if (score < 0) score = 0;
    const char *grade = score >= 85 ? "HEALTHY"
                      : score >= 65 ? "BUSY"
                      : score >= 45 ? "STRESSED"
                      : "OVERLOADED";

    printf("[ROCKY]  CPU score : %d/100  [%s]  signals=%d\n",
           score, grade, sigs);

    write_results(score, scan_num, sigs, sys_pct, ncores, throttled_count);
}

/* ?? Main ??????????????????????????????????????????????????????????????? */

int main(void)
{
    bexec_init();

    if (!is_enabled()) {
        printf("[ROCKY] disabled via syndicatectl -- exiting\n");
        return 0;
    }

    printf("[ROCKY] CPU Load, Frequency & Cluster Balance Daemon: ONLINE\n");
    printf("[ROCKY] HOG threshold: %d%%  Critical: %d%%\n",
           HOG_PCT, HOG_CRITICAL_PCT);

    int interval  = get_interval();
    int max_scans = get_max_scans();
    int scan_num  = 0;

    for (;;) {
        if (!is_enabled()) {
            printf("[ROCKY] disabled -- stopping\n");
            break;
        }

        interval  = get_interval();
        max_scans = get_max_scans();
        scan_num++;

        poll(scan_num);

        if (max_scans > 0 && scan_num >= max_scans) {
            printf("[ROCKY] reached scan_count=%d -- exiting\n", max_scans);
            break;
        }

        printf("[ROCKY] Next scan in %ds\n", interval);
        sleep(interval);
    }

    return 0;
}
