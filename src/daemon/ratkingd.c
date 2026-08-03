/*
 * ratkingd v2.0 — Zombie Process & Behavioral Anomaly Hunter
 * CSI Mode: baseline learning, hidden process detection, thermal-aware thresholds,
 *            network correlation, memory attribution, lifecycle tracking
 */

#include "daemon_core.h"
#include "ipc_globals.h"
#include <stdbool.h>

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
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>

#define DAEMON_NAME     "ratkingd"
#define VERSION         "2.0"
#define POLL_SEC        12
#define TOP_N           6
#define HOG_PCT_COOL    40
#define HOG_PCT_WARM    25
#define HOG_PCT_HOT     15
#define MEM_LOW_MB      200
#define MAX_PROCS       512
#define MAX_BASELINE    128
#define LEARN_POLLS     20
#define FLASH_LIFETIME  5
#define ANOMALY_SIGMA   3.0

static volatile bool g_ratkingd_running = true;

/* ── Process record with lifecycle ───────────────────────────────────────── */
typedef struct {
    int   pid;
    char  name[32];
    char  state;
    int   uid;
    int   ppid;
    long  vmrss_kb;
    unsigned long long cpu_time;
    int   cpu_pct;
    time_t first_seen;
    time_t last_seen;
    int   polls_alive;
    int   fd_count;
    long  tx_kb;
    long  rx_kb;
} proc_t;

static proc_t g_procs[MAX_PROCS];
static proc_t g_prev[MAX_PROCS];
static int    g_nprocs = 0, g_nprev = 0;
static int    g_first = 1;
static long long g_prev_total_jiff = 0;

/* ── Behavioral baseline ──────────────────────────────────────────────────── */
typedef struct {
    char name[32];
    double cpu_mean;
    double cpu_stddev;
    double rss_mean;
    double rss_stddev;
    int samples;
    int max_cpu;
    long max_rss;
} baseline_t;

static baseline_t g_baselines[MAX_BASELINE];
static int g_nbaseline = 0;
static int g_learn_polls = 0;
static int g_baseline_ready = 0;

/* ── JSON output path ─────────────────────────────────────────────────────── */
#define RESULTS_FILE "/data/data/com.termux/files/home/MiuiserPeruser/Registry/daemon_results/ratkingd.json"

/* ── Unified logging ──────────────────────────────────────────────────────
 * Format: [ISO8601][RATKINGD/LEVEL] message. Always writes to stderr; also
 * writes to a log file if RATKING_LOG_PATH is set. Separate va_start/va_end
 * per output destination -- reusing one va_list across two vfprintf() calls
 * is undefined behavior. */
static FILE *g_ratking_log_fp = NULL;

static void ratkinglog_init(void) {
    const char *path = getenv("RATKING_LOG_PATH");
    if (path && *path) {
        g_ratking_log_fp = fopen(path, "a");
        if (!g_ratking_log_fp) {
            fprintf(stderr, "[RATKINGD] WARN: cannot open log file %s: %s\n",
                    path, strerror(errno));
        }
    }
}

static void ratkinglog(const char *level, const char *fmt, ...) {
    time_t t = time(NULL);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", localtime(&t));

    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[%s][RATKINGD/%s] ", ts, level);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);

    if (g_ratking_log_fp) {
        va_list ap2;
        va_start(ap2, fmt);
        fprintf(g_ratking_log_fp, "[%s][RATKINGD/%s] ", ts, level);
        vfprintf(g_ratking_log_fp, fmt, ap2);
        fprintf(g_ratking_log_fp, "\n");
        va_end(ap2);
        fflush(g_ratking_log_fp);
    }
}

/* JSON-escape a free-text string (process names from /proc, cgroup content)
 * before splicing into hand-built JSON -- none of that text is trustworthy
 * input, and an embedded quote/backslash/newline would silently corrupt the
 * JSON output (same class of bug fixed via json_escape() in fugitoidd.c
 * and metalheadd.c this arc). */
static void json_escape(const char *in, char *out, size_t out_size) {
    if (!in || out_size == 0) { if (out_size) out[0] = '\0'; return; }
    size_t j = 0;
    for (size_t i = 0; in[i] != '\0' && j + 1 < out_size; i++) {
        unsigned char c = (unsigned char)in[i];
        if (c == '"' || c == '\\') {
            if (j + 2 >= out_size) break;
            out[j++] = '\\';
            out[j++] = (char)c;
        } else if (c == '\n') {
            if (j + 2 >= out_size) break;
            out[j++] = '\\'; out[j++] = 'n';
        } else if (c == '\r') {
            if (j + 2 >= out_size) break;
            out[j++] = '\\'; out[j++] = 'r';
        } else if (c == '\t') {
            if (j + 2 >= out_size) break;
            out[j++] = '\\'; out[j++] = 't';
        } else if (c < 0x20) {
            continue;  /* drop other control chars */
        } else {
            out[j++] = (char)c;
        }
    }
    out[j] = '\0';
}

/* ── rish helper ───────────────────────────────────────────────────────────── */
static char *rish_read(const char *cmd, char *buf, size_t bufsize) {
    char *result = bexec(cmd);
    if (!result) { buf[0] = 0; return buf; }

    strncpy(buf, result, bufsize - 1);
    buf[bufsize - 1] = 0;
    free(result);

    size_t pos = strlen(buf);
    while (pos > 0 && (buf[pos-1] == '\n' || buf[pos-1] == '\r')) buf[--pos] = 0;
    return buf;
}

static int contains(const char *s, const char *needle) {
    return s && needle && strstr(s, needle) != NULL;
}

/* ── Thermal state ───────────────────────────────────────────────────────── */
static int get_thermal_temp(void) {
    FILE *f = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (!f) return -1;
    int temp = -1;
    fscanf(f, "%d", &temp);
    fclose(f);
    return temp / 1000;  /* millidegrees → degrees */
}

/* Thermal-based threshold adjustment removed — confirmed not a kill predictor
 * on this device (FPSGO frequency oscillation, not thermal, drives behavior).
 * Temp is still read and logged for context, but no longer adjusts hog_pct. */
static int get_adjusted_hog_pct(int temp) {
    (void)temp;
    return HOG_PCT_COOL;
}

/* ── Baseline management ──────────────────────────────────────────────────── */
static baseline_t* find_baseline(const char *name) {
    for (int i = 0; i < g_nbaseline; i++)
        if (strcmp(g_baselines[i].name, name) == 0)
            return &g_baselines[i];
    return NULL;
}

static void update_baseline(const char *name, int cpu_pct, long rss_kb) {
    baseline_t *b = find_baseline(name);
    if (!b && g_nbaseline < MAX_BASELINE) {
        b = &g_baselines[g_nbaseline++];
        strcpy(b->name, name);
        b->cpu_mean = cpu_pct;
        b->cpu_stddev = 0;
        b->rss_mean = rss_kb;
        b->rss_stddev = 0;
        b->samples = 1;
        b->max_cpu = cpu_pct;
        b->max_rss = rss_kb;
        return;
    }
    if (!b) return;

    /* Welford's online algorithm for mean/stddev */
    b->samples++;
    double delta_cpu = cpu_pct - b->cpu_mean;
    b->cpu_mean += delta_cpu / b->samples;
    b->cpu_stddev = sqrt((b->cpu_stddev * b->cpu_stddev * (b->samples - 1) +
                          delta_cpu * (cpu_pct - b->cpu_mean)) / b->samples);

    double delta_rss = rss_kb - b->rss_mean;
    b->rss_mean += delta_rss / b->samples;
    b->rss_stddev = sqrt((b->rss_stddev * b->rss_stddev * (b->samples - 1) +
                          delta_rss * (rss_kb - b->rss_mean)) / b->samples);

    if (cpu_pct > b->max_cpu) b->max_cpu = cpu_pct;
    if (rss_kb > b->max_rss) b->max_rss = rss_kb;
}

static int is_anomaly_cpu(const char *name, int cpu_pct) {
    baseline_t *b = find_baseline(name);
    if (!b || b->samples < 5) return 0;
    return cpu_pct > b->cpu_mean + ANOMALY_SIGMA * b->cpu_stddev;
}

static int is_anomaly_rss(const char *name, long rss_kb) {
    baseline_t *b = find_baseline(name);
    if (!b || b->samples < 5) return 0;
    return rss_kb > b->rss_mean + ANOMALY_SIGMA * b->rss_stddev;
}

/* ── /proc readers with PPID and network ──────────────────────────────────── */
static int read_status(int pid, proc_t *out) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char line[256];
    out->pid = pid;
    out->state = '?';
    out->uid = -1;
    out->ppid = -1;
    out->vmrss_kb = 0;
    out->name[0] = '\0';

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "Name:", 5) == 0) {
            sscanf(line + 5, " %31s", out->name);
        } else if (strncmp(line, "State:", 6) == 0) {
            sscanf(line + 6, " %c", &out->state);
        } else if (strncmp(line, "Uid:", 4) == 0) {
            sscanf(line + 4, " %d", &out->uid);
        } else if (strncmp(line, "PPid:", 5) == 0) {
            sscanf(line + 5, " %d", &out->ppid);
        } else if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line + 6, " %ld", &out->vmrss_kb);
        }
    }
    fclose(f);
    return 1;
}

static unsigned long long read_cputime(int pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    unsigned long long utime = 0, stime = 0, dummy;
    char name[64]; char state;
    int ppid, pgrp, sess, tty, tpgid;
    unsigned flags;
    fscanf(f, "%*d %63s %c %d %d %d %d %d %u %llu %llu %llu %llu %llu %llu",
           name, &state, &ppid, &pgrp, &sess, &tty, &tpgid, &flags,
           &dummy, &dummy, &dummy, &dummy, &utime, &stime);
    fclose(f);
    return utime + stime;
}

static int read_fd_count(int pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/fd/", pid);
    DIR *d = opendir(path);
    if (!d) return 0;
    int count = 0;
    while (readdir(d) != NULL) count++;
    closedir(d);
    return count - 2;  /* subtract . and .. */
}

static void read_network(int pid, long *tx, long *rx) {
    *tx = 0; *rx = 0;
    char path[128];
    snprintf(path, sizeof(path), "/proc/%d/net/dev", pid);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strchr(line, ':')) {
            char iface[32];
            long rbytes, tbytes;
            sscanf(line, " %31[^:]: %ld %*d %*d %*d %*d %*d %*d %*d %ld",
                   iface, &rbytes, &tbytes);
            if (strcmp(iface, "lo") != 0) {
                *rx += rbytes / 1024;
                *tx += tbytes / 1024;
            }
        }
    }
    fclose(f);
}

static long long total_jiffies(void) {
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return 0;
    char line[256];
    long long u=0,n=0,s=0,i=0,w=0,q=0,sq=0;
    fgets(line, sizeof(line), f);
    fclose(f);
    sscanf(line, "cpu %lld %lld %lld %lld %lld %lld %lld",
           &u,&n,&s,&i,&w,&q,&sq);
    return u+n+s+i+w+q+sq;
}

static void read_meminfo(long *total_kb, long *avail_kb) {
    *total_kb = *avail_kb = 0;
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "MemTotal:", 9) == 0)
            sscanf(line + 9, " %ld", total_kb);
        else if (strncmp(line, "MemAvailable:", 13) == 0)
            sscanf(line + 13, " %ld", avail_kb);
    }
    fclose(f);
}

/* REMOVED 2026-07-28: detect_hidden_processes() computed
 * gap = max_pid - actual /proc entry count and treated any gap > 5 as
 * evidence of hidden processes. This is invalid, not just miscalibrated:
 * PIDs aren't dense (the kernel increments a running counter up to
 * pid_max, reusing old numbers as processes exit), so a high max_pid
 * just reflects total process churn since boot, not expected live-process
 * count. Confirmed live: the gap grew from 31678 to 32714 across ~15 polls
 * while actual process count held steady at 6-7, firing HIDDEN_PROCESS on
 * every single poll, permanently, on a completely normal device. The
 * underlying idea (detecting /proc-hiding rootkits) is legitimate but
 * needs a second independent enumeration method (e.g. diffing against
 * /proc/sched_debug) to compare against the /proc listing -- comparing
 * against max_pid was never a valid implementation of that idea. Removed
 * rather than threshold-tuned, since the gap value itself is meaningless
 * regardless of cutoff. */

/* ── ProcessManager kill correlation ─────────────────────────────────────── */
static int g_pm_kill_count_prev = -1;
static time_t g_pm_last_kill_time = 0;

static int check_pm_kill_storm(char *out_json, size_t out_size) {
    char *dump = bexec("dumpsys activity service ProcessManager");
    out_json[0] = 0;
    if (!dump) return 0;

    int count = 0;
    char *p = dump;
    while ((p = strstr(p, "mKillingPackageMaps")) != NULL) { count++; p += 20; }

    int storm = 0;
    if (g_pm_kill_count_prev >= 0 && count > g_pm_kill_count_prev + 10) {
        storm = 1;
        g_pm_last_kill_time = time(NULL);
    }
    g_pm_kill_count_prev = count;

    int recent = (time(NULL) - g_pm_last_kill_time) < 300;

    snprintf(out_json, out_size,
        "{\"type\":\"pm_kill_storm\",\"count\":%d,\"storm_active\":%s,\"recent\":%s}",
        count, storm ? "true" : "false", recent ? "true" : "false");

    free(dump);
    return storm;
}

/* ── PeriodicCleaner rearm detection ──────────────────────────────────────── */
static int g_periodic_enabled_prev = -1;

static int check_periodic_rearm(char *out_json, size_t out_size) {
    char *dump = bexec("cmd periodic dump");
    out_json[0] = 0;
    if (!dump) return 0;

    int enabled = contains(dump, "Enable=true") ? 1 : 0;
    int rearmed = (g_periodic_enabled_prev == 0 && enabled == 1);
    g_periodic_enabled_prev = enabled;

    snprintf(out_json, out_size,
        "{\"type\":\"periodic_rearm\",\"enabled\":%s,\"rearmed\":%s}",
        enabled ? "true" : "false", rearmed ? "true" : "false");

    free(dump);
    return rearmed;
}

/* ── MILLET module health ─────────────────────────────────────────────────── */
static int check_millet_health(char *out_json, size_t out_size) {
    char *mods = bexec("cat /proc/modules 2>/dev/null | grep -c '^millet_'");
    out_json[0] = 0;
    if (!mods) return 0;

    int count = atoi(mods);
    int degraded = count < 5;

    snprintf(out_json, out_size,
        "{\"type\":\"millet_health\",\"module_count\":%d,\"degraded\":%s}",
        count, degraded ? "true" : "false");

    free(mods);
    return degraded;
}

/* ── Self-protection status ───────────────────────────────────────────────── */
static int check_self_protection(char *out_json, size_t out_size) {
    FILE *f = fopen("/proc/self/oom_score_adj", "r");
    int oom_adj = 0;
    if (f) { fscanf(f, "%d", &oom_adj); fclose(f); }

    char cgroup[256] = "";
    f = fopen("/proc/self/cgroup", "r");
    if (f) { fgets(cgroup, sizeof(cgroup), f); fclose(f); }
    cgroup[strcspn(cgroup, "\n")] = '\0';

    int unprotected = (oom_adj > -900);

    char esc_cgroup[400];
    json_escape(cgroup, esc_cgroup, sizeof(esc_cgroup));

    out_json[0] = 0;
    snprintf(out_json, out_size,
        "{\"type\":\"self_protection\",\"oom_score_adj\":%d,\"cgroup\":\"%.300s\",\"unprotected\":%s}",
        oom_adj, esc_cgroup, unprotected ? "true" : "false");

    return unprotected;
}

/* ── JSON helpers ─────────────────────────────────────────────────────────── */
static void strip_trailing_comma(char *s) {
    size_t len = strlen(s);
    if (len > 0 && s[len - 1] == ',') s[len - 1] = 0;
}

static void write_json(
    const char *ts, int temp, int hog_pct,
    int total_procs, int zombies, int orphans, int flash,
    const char *top_cpu_json, const char *top_mem_json,
    const char *anomalies_json, const char *pressure_json,
    const char *network_json)
{
    char tc[4096], tm[4096], an[4096], pr[4096], nw[4096];
    strncpy(tc, top_cpu_json, sizeof(tc)-1); tc[sizeof(tc)-1] = 0;
    strncpy(tm, top_mem_json, sizeof(tm)-1); tm[sizeof(tm)-1] = 0;
    strncpy(an, anomalies_json, sizeof(an)-1); an[sizeof(an)-1] = 0;
    strncpy(pr, pressure_json, sizeof(pr)-1); pr[sizeof(pr)-1] = 0;
    strncpy(nw, network_json, sizeof(nw)-1); nw[sizeof(nw)-1] = 0;
    strip_trailing_comma(tc);
    strip_trailing_comma(tm);
    strip_trailing_comma(an);
    strip_trailing_comma(pr);
    strip_trailing_comma(nw);

    FILE *f = fopen(RESULTS_FILE, "w");
    if (!f) return;

    fprintf(f,
        "{\n"
        "  \"daemon\": \"ratkingd\",\n"
        "  \"version\": \"%s\",\n"
        "  \"timestamp\": \"%s\",\n"
        "  \"poll_interval_sec\": %d,\n\n"
        "  \"baseline\": {\n"
        "    \"established\": %s,\n"
        "    \"polls_learned\": %d,\n"
        "    \"process_types_tracked\": %d\n"
        "  },\n\n"
        "  \"thermal\": {\n"
        "    \"zone_temp_c\": %d,\n"
        "    \"adjusted_hog_pct\": %d\n"
        "  },\n\n"
        "  \"processes\": {\n"
        "    \"total\": %d,\n"
        "    \"zombies\": %d,\n"
        "    \"orphans\": %d,\n"
        "    \"flash_processes\": %d\n"
        "  },\n\n"
        "  \"top_cpu\": [%s],\n\n"
        "  \"top_memory\": [%s],\n\n"
        "  \"anomalies\": [%s],\n\n"
        "  \"pressure\": %s,\n\n"
        "  \"network_correlation\": [%s]\n"
        "}\n",
        VERSION, ts, POLL_SEC,
        g_baseline_ready ? "true" : "false", g_learn_polls, g_nbaseline,
        temp, hog_pct,
        total_procs, zombies, orphans, flash,
        tc, tm, an, pr, nw);

    fflush(f);
    fclose(f);
}

/* ── Main poll ─────────────────────────────────────────────────────────────── */
static int cmp_cpu(const void *a, const void *b) {
    return ((proc_t*)b)->cpu_pct - ((proc_t*)a)->cpu_pct;
}
static int cmp_mem(const void *a, const void *b) {
    return (int)(((proc_t*)b)->vmrss_kb - ((proc_t*)a)->vmrss_kb);
}

static void poll_procs(void) {
    DIR *d = opendir("/proc");
    if (!d) return;

    long long cur_total_j = total_jiffies();
    long long delta_j = (cur_total_j > g_prev_total_jiff)
                      ? cur_total_j - g_prev_total_jiff : 1;

    struct dirent *ent;
    g_nprocs = 0;
    int zombie_count = 0;
    int orphan_count = 0;
    int flash_count = 0;

    while ((ent = readdir(d)) != NULL && g_nprocs < MAX_PROCS) {
        int is_pid = 1;
        for (char *c = ent->d_name; *c; c++)
            if (*c < '0' || *c > '9') { is_pid = 0; break; }
        if (!is_pid) continue;

        int pid = atoi(ent->d_name);
        proc_t *p = &g_procs[g_nprocs];
        if (!read_status(pid, p)) continue;
        p->cpu_time = read_cputime(pid);
        p->fd_count = read_fd_count(pid);
        read_network(pid, &p->tx_kb, &p->rx_kb);

        /* CPU% from delta */
        p->cpu_pct = 0;
        if (!g_first) {
            for (int j = 0; j < g_nprev; j++) {
                if (g_prev[j].pid == pid) {
                    unsigned long long dt = (p->cpu_time > g_prev[j].cpu_time)
                                          ? p->cpu_time - g_prev[j].cpu_time : 0;
                    p->cpu_pct = (int)(dt * 100ULL / (unsigned long long)delta_j);
                    break;
                }
            }
        }

        /* Lifecycle tracking */
        p->last_seen = time(NULL);
        if (!g_first) {
            int found_prev = 0;
            for (int j = 0; j < g_nprev; j++) {
                if (g_prev[j].pid == pid) {
                    p->first_seen = g_prev[j].first_seen;
                    p->polls_alive = g_prev[j].polls_alive + 1;
                    found_prev = 1;
                    break;
                }
            }
            if (!found_prev) {
                p->first_seen = p->last_seen;
                p->polls_alive = 1;
            }
        } else {
            p->first_seen = p->last_seen;
            p->polls_alive = 1;
        }

        /* Flash process detection */
        int lifetime = (int)(p->last_seen - p->first_seen);
        if (lifetime > 0 && lifetime < FLASH_LIFETIME && p->cpu_pct > 10)
            flash_count++;

        /* Orphan detection (PPID=1 but not init/system service) */
        if (p->ppid == 1 && p->pid > 100 && p->uid > 1000)
            orphan_count++;

        if (p->state == 'Z') zombie_count++;
        g_nprocs++;
    }
    closedir(d);

    char pm_json[512], periodic_json[512], millet_json[512], selfprot_json[512];
    int pm_storm = check_pm_kill_storm(pm_json, sizeof(pm_json));
    int periodic_rearmed = check_periodic_rearm(periodic_json, sizeof(periodic_json));
    int millet_degraded = check_millet_health(millet_json, sizeof(millet_json));
    int self_unprotected = check_self_protection(selfprot_json, sizeof(selfprot_json));

    if (pm_storm) gaveld_emit(DAEMON_NAME, "PM_KILL_STORM", 0.85, pm_json);
    if (periodic_rearmed) gaveld_emit(DAEMON_NAME, "PERIODIC_REARM_NEEDED", 0.7, periodic_json);
    if (millet_degraded) gaveld_emit(DAEMON_NAME, "MILLET_DEGRADED", 0.75, millet_json);
    if (self_unprotected) gaveld_emit(DAEMON_NAME, "SELF_UNPROTECTED", 0.8, selfprot_json);

    /* Save for next round */
    memcpy(g_prev, g_procs, (size_t)g_nprocs * sizeof(proc_t));
    g_nprev = g_nprocs;
    g_prev_total_jiff = cur_total_j;
    g_first = 0;

    /* Memory */
    long total_kb, avail_kb;
    read_meminfo(&total_kb, &avail_kb);
    long avail_mb = avail_kb / 1024;

    /* Thermal */
    int temp = get_thermal_temp();
    int hog_pct = get_adjusted_hog_pct(temp);

    /* Sort */
    proc_t sorted_cpu[MAX_PROCS], sorted_mem[MAX_PROCS];
    memcpy(sorted_cpu, g_procs, (size_t)g_nprocs * sizeof(proc_t));
    memcpy(sorted_mem, g_procs, (size_t)g_nprocs * sizeof(proc_t));
    qsort(sorted_cpu, (size_t)g_nprocs, sizeof(proc_t), cmp_cpu);
    qsort(sorted_mem, (size_t)g_nprocs, sizeof(proc_t), cmp_mem);

    /* Build JSON components */
    char top_cpu_json[4096] = "", top_mem_json[4096] = "";
    char anomalies_json[4096] = "";
    char pressure_json[512];
    snprintf(pressure_json, sizeof(pressure_json),
        "{\"memory_low\":false,\"avail_mb\":%ld,\"attributed_to\":\"\"}",
        avail_mb);
    char network_json[4096] = "";

    int shown = 0;
    for (int i = 0; i < g_nprocs && shown < TOP_N; i++) {
        if (sorted_cpu[i].cpu_pct <= 0 && shown > 0) break;

        /* Update baseline */
        if (g_learn_polls < LEARN_POLLS) {
            update_baseline(sorted_cpu[i].name, sorted_cpu[i].cpu_pct, sorted_cpu[i].vmrss_kb);
        }

        /* Top CPU JSON */
        int anomaly_cpu = is_anomaly_cpu(sorted_cpu[i].name, sorted_cpu[i].cpu_pct);
        baseline_t *b = find_baseline(sorted_cpu[i].name);
        char esc_name[80];
        json_escape(sorted_cpu[i].name, esc_name, sizeof(esc_name));
        char entry[512];
        snprintf(entry, sizeof(entry),
            "{\"pid\":%d,\"name\":\"%s\",\"cpu_pct\":%d,\"baseline_mean\":%.1f,\"anomaly\":%s},",
            sorted_cpu[i].pid, esc_name, sorted_cpu[i].cpu_pct,
            b ? b->cpu_mean : 0.0,
            anomaly_cpu ? "true" : "false");
        strncat(top_cpu_json, entry, sizeof(top_cpu_json) - 1);

        /* Anomaly detection */
        if (anomaly_cpu || is_anomaly_rss(sorted_cpu[i].name, sorted_cpu[i].vmrss_kb)) {
            snprintf(entry, sizeof(entry),
                "{\"type\":\"%s\",\"pid\":%d,\"name\":\"%s\",\"cpu_pct\":%d,\"rss_mb\":%.1f,\"confidence\":%d,\"thermal\":\"%s\"},",
                anomaly_cpu ? "cpu_spike" : "rss_spike",
                sorted_cpu[i].pid, esc_name,
                sorted_cpu[i].cpu_pct, sorted_cpu[i].vmrss_kb / 1024.0,
                90, temp > 60 ? "HOT" : temp > 45 ? "WARM" : "COOL");
            strncat(anomalies_json, entry, sizeof(anomalies_json) - 1);
        }

        shown++;
    }

    /* Top memory JSON */
    shown = 0;
    for (int i = 0; i < g_nprocs && shown < TOP_N; i++) {
        if (sorted_mem[i].vmrss_kb <= 0) break;
        int anomaly_rss = is_anomaly_rss(sorted_mem[i].name, sorted_mem[i].vmrss_kb);
        char esc_name[80];
        json_escape(sorted_mem[i].name, esc_name, sizeof(esc_name));
        char entry[512];
        snprintf(entry, sizeof(entry),
            "{\"pid\":%d,\"name\":\"%s\",\"rss_mb\":%.1f,\"anomaly\":%s},",
            sorted_mem[i].pid, esc_name,
            sorted_mem[i].vmrss_kb / 1024.0,
            anomaly_rss ? "true" : "false");
        strncat(top_mem_json, entry, sizeof(top_mem_json) - 1);
        shown++;
    }

    /* Network correlation */
    for (int i = 0; i < g_nprocs; i++) {
        if (g_procs[i].cpu_pct > 20 && (g_procs[i].tx_kb > 1024 || g_procs[i].rx_kb > 1024)) {
            char esc_name[80];
            json_escape(g_procs[i].name, esc_name, sizeof(esc_name));
            char entry[512];
            snprintf(entry, sizeof(entry),
                "{\"pid\":%d,\"name\":\"%s\",\"cpu_pct\":%d,\"tx_kb\":%ld,\"rx_kb\":%ld,\"correlation\":\"high\"},",
                g_procs[i].pid, esc_name, g_procs[i].cpu_pct,
                g_procs[i].tx_kb, g_procs[i].rx_kb);
            strncat(network_json, entry, sizeof(network_json) - 1);
        }
    }

    /* Memory pressure attribution */
    if (avail_mb < MEM_LOW_MB && avail_mb > 0) {
        /* Find largest RSS grower */
        long max_delta = 0;
        char *attributed = "unknown";
        for (int i = 0; i < g_nprocs; i++) {
            for (int j = 0; j < g_nprev; j++) {
                if (g_prev[j].pid == g_procs[i].pid) {
                    long delta = g_procs[i].vmrss_kb - g_prev[j].vmrss_kb;
                    if (delta > max_delta) {
                        max_delta = delta;
                        attributed = g_procs[i].name;
                    }
                    break;
                }
            }
        }
        char pr[512];
        char esc_attrib[80];
        json_escape(attributed, esc_attrib, sizeof(esc_attrib));
        snprintf(pr, sizeof(pr),
            "{\"memory_low\":true,\"avail_mb\":%ld,\"attributed_to\":\"%s (+%.1fMB in last poll)\"}",
            avail_mb, esc_attrib, max_delta / 1024.0);
        strncpy(pressure_json, pr, sizeof(pressure_json) - 1);
        pressure_json[sizeof(pressure_json) - 1] = 0;

        gaveld_emit(DAEMON_NAME, "MEM_PRESSURE", 0.0, pressure_json);
    }

    /* APRIL events */
    if (zombie_count > 0) {
        char ev[256];
        snprintf(ev, sizeof(ev), "zombie_count=%d", zombie_count);
        gaveld_emit(DAEMON_NAME, "ZOMBIE_DETECTED", 0.0, ev);
    }
    if (sorted_cpu[0].cpu_pct >= hog_pct) {
        char ev[256];
        snprintf(ev, sizeof(ev), "pid=%d name=%s cpu_pct=%d temp=%d",
                 sorted_cpu[0].pid, sorted_cpu[0].name, sorted_cpu[0].cpu_pct, temp);
        gaveld_emit(DAEMON_NAME, "CPU_HOG", 0.0, ev);
    }
    if (flash_count > 0) {
        char ev[256];
        snprintf(ev, sizeof(ev), "flash_count=%d", flash_count);
        gaveld_emit(DAEMON_NAME, "FLASH_PROCESS", 0.0, ev);
    }

    /* Console report */
    char ts[32];
    time_t t = time(NULL);
    strftime(ts, sizeof(ts), "%H:%M:%S", localtime(&t));

    printf("\n[RATKING] ── %s ── temp=%d°C hog_pct=%d%% ──\n", ts, temp, hog_pct);
    printf("[RATKING]  Procs: %d  Zombies: %d  Orphans: %d  Flash: %d\n",
           g_nprocs, zombie_count, orphan_count, flash_count);
    printf("[RATKING]  Memory: %ldMB free  Baseline: %s (%d/%d polls)\n",
           avail_mb, g_baseline_ready ? "ready" : "learning", g_learn_polls, LEARN_POLLS);

    if (strlen(anomalies_json) > 0)
        printf("[RATKING]  ANOMALIES detected\n");
    if (flash_count > 0)
        printf("[RATKING]  *** FLASH: %d short-lived high-CPU processes ***\n", flash_count);

    fflush(stdout);

    /* Write JSON */
    write_json(ts, temp, hog_pct, g_nprocs, zombie_count, orphan_count,
               flash_count, top_cpu_json, top_mem_json,
               anomalies_json, pressure_json, network_json);

    g_learn_polls++;
    if (g_learn_polls >= LEARN_POLLS && !g_baseline_ready) {
        g_baseline_ready = 1;
        ratkinglog("INFO", "Baseline ready: %d process types tracked", g_nbaseline);
    }
}

/* ── Main ─────────────────────────────────────────────────────────────────── */
int main(void) {
    if (!daemon_core_init(DAEMON_NAME)) return 1;
    ratkinglog_init();
    ratkinglog("INFO", "v%s Process & Behavioral Anomaly Hunter: ONLINE", VERSION);
    ratkinglog("INFO", "Learning baseline for %d polls...", LEARN_POLLS);

    for (;;) {
        poll_procs();
        sleep(POLL_SEC);
    }

    if (g_ratking_log_fp) fclose(g_ratking_log_fp);
    daemon_core_shutdown();
    return 0;
}
