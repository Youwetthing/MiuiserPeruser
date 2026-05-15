/*
 * rocksteadyd.c — CPU Frequency, Load & Throttle Monitor
 *
 * Every poll:
 *   - Read per-core current/min/max freq and governor from /sys
 *   - Parse /proc/stat to compute per-core and aggregate CPU utilisation
 *   - Detect throttling (cur_freq < 30% of max), governor anomalies
 *   - Track sustained-load streaks and emit APRIL events
 *
 * APRIL events emitted:
 *   cpu_throttle  — core running at < THROTTLE_PCT of max freq
 *   cpu_load_high — sustained aggregate load above LOAD_HIGH_PCT
 */

#include "daemon_core.h"
#include "ipc_globals.h"
#include "backend_exec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/un.h>

#define DAEMON_NAME      "rocksteadyd"
#define POLL_SEC         8
#define MAX_CPUS         16
#define THROTTLE_PCT     30   /* cur_freq / max_freq < this → throttled */
#define LOAD_HIGH_PCT    80   /* aggregate utilisation alert threshold */
#define LOAD_HIGH_STREAK 3    /* consecutive high-load polls before APRIL */

/* ── Splinterd emit ───────────────────────────────────────────────────── */

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

/* ── /proc/stat CPU jiffies ───────────────────────────────────────────── */

typedef struct { unsigned long long u, n, s, i, w, q, sq; } jiffies_t;

static jiffies_t g_prev_j[MAX_CPUS + 1];   /* [0] = total, [1..] = per-core */
static int       g_first_stat = 1;

static int read_stat(jiffies_t *out_cores, int *ncores_out)
{
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return 0;
    int nc = 0;
    char line[256];
    *ncores_out = 0;
    while (fgets(line, sizeof(line), f) && nc <= MAX_CPUS) {
        jiffies_t j = {0};
        char label[16];
        if (sscanf(line, "%15s %llu %llu %llu %llu %llu %llu %llu",
                   label, &j.u, &j.n, &j.s, &j.i, &j.w, &j.q, &j.sq) < 5)
            continue;
        if (strncmp(label, "cpu", 3) != 0) continue;
        out_cores[nc++] = j;
        if (label[3] >= '0')    /* skip the aggregate "cpu" line at nc==0 */
            (*ncores_out)++;
    }
    fclose(f);
    return nc;
}

/* Compute utilisation % from consecutive jiffies snapshots */
static int util_pct(const jiffies_t *a, const jiffies_t *b)
{
    unsigned long long active_a = a->u + a->n + a->s + a->q + a->sq;
    unsigned long long active_b = b->u + b->n + b->s + b->q + b->sq;
    unsigned long long total_a  = active_a + a->i + a->w;
    unsigned long long total_b  = active_b + b->i + b->w;
    unsigned long long da = (active_b > active_a) ? active_b - active_a : 0;
    unsigned long long dt = (total_b  > total_a ) ? total_b  - total_a  : 1;
    return (int)(da * 100ULL / dt);
}

/* ── Read sysfs freq for one CPU ──────────────────────────────────────── */

typedef struct {
    int  online;
    long cur_khz;
    long max_khz;
    long min_khz;
    char governor[32];
    int  throttled;
    int  util_pct;  /* from /proc/stat */
} cpu_info_t;

static cpu_info_t g_cpu[MAX_CPUS];
static int        g_ncpus = 0;

/* Route all sysfs reads through bexec_read_file (fopen → privileged cat) */
static long read_long_file(const char *path)
{
    char *s = bexec_read_file(path);
    if (!s) return -1;
    long v = atol(s);
    free(s);
    return v;
}

static void read_governor(int cpu_id, char *out, size_t outlen)
{
    char path[128];
    snprintf(path, sizeof(path),
             "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor", cpu_id);
    char *s = bexec_read_file(path);
    if (!s) { strncpy(out, "N/A", outlen); return; }
    strncpy(out, s, outlen - 1);
    out[outlen - 1] = '\0';
    out[strcspn(out, "\n")] = '\0';
    free(s);
}

/* ── Discover CPUs ────────────────────────────────────────────────────── */

static void discover_cpus(void)
{
    g_ncpus = 0;
    for (int i = 0; i < MAX_CPUS; i++) {
        char path[128];
        snprintf(path, sizeof(path),
                 "/sys/devices/system/cpu/cpu%d", i);
        if (access(path, F_OK) != 0) break;
        g_ncpus = i + 1;
    }
}

/* ── Poll ─────────────────────────────────────────────────────────────── */

static int g_high_streak = 0;
static int g_cycle       = 0;

static void poll_cpu(void)
{
    /* /proc/stat snapshot */
    jiffies_t curr_j[MAX_CPUS + 1];
    int nstat_cpus = 0;
    int nstat = read_stat(curr_j, &nstat_cpus);

    /* Per-core freq */
    int throttle_count = 0;
    for (int i = 0; i < g_ncpus; i++) {
        cpu_info_t *c = &g_cpu[i];
        char path[128];

        /* Online status */
        snprintf(path, sizeof(path),
                 "/sys/devices/system/cpu/cpu%d/online", i);
        long online_v = read_long_file(path);
        c->online = (i == 0) ? 1 : (online_v >= 1 ? 1 : 0);

        if (!c->online) continue;

        snprintf(path, sizeof(path),
                 "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", i);
        c->cur_khz = read_long_file(path);

        snprintf(path, sizeof(path),
                 "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_max_freq", i);
        c->max_khz = read_long_file(path);

        snprintf(path, sizeof(path),
                 "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_min_freq", i);
        c->min_khz = read_long_file(path);

        read_governor(i, c->governor, sizeof(c->governor));

        c->throttled = (c->cur_khz > 0 && c->max_khz > 0 &&
                        (c->cur_khz * 100L / c->max_khz) < THROTTLE_PCT) ? 1 : 0;
        if (c->throttled) throttle_count++;

        /* /proc/stat utilisation for this core (index offset +1 for "cpu" total) */
        int si = i + 1;  /* curr_j[0]=aggregate, curr_j[1]=cpu0 ... */
        if (!g_first_stat && si < nstat)
            c->util_pct = util_pct(&g_prev_j[si], &curr_j[si]);
        else
            c->util_pct = -1;
    }

    /* Aggregate utilisation */
    int agg_pct = -1;
    if (!g_first_stat && nstat > 0)
        agg_pct = util_pct(&g_prev_j[0], &curr_j[0]);

    /* Save for next round */
    if (nstat > 0) {
        memcpy(g_prev_j, curr_j,
               (size_t)nstat * sizeof(jiffies_t));
        g_first_stat = 0;
    }

    /* Print */
    char ts[32];
    time_t t = time(NULL);
    strftime(ts, sizeof(ts), "%H:%M:%S", localtime(&t));

    printf("\n[ROCKY] ── CPU Status  %s  (cycle #%d) ────────────────────\n",
           ts, ++g_cycle);
    printf("[ROCKY]  %-4s  %-7s  %-8s  %-8s  %-8s  %-14s  %-5s  %s\n",
           "Core","Status ","Cur MHz","Max MHz","Min MHz",
           "Governor      ","Load%","Throttle");
    printf("[ROCKY]  ────────────────────────────────────────────────────────\n");

    for (int i = 0; i < g_ncpus; i++) {
        cpu_info_t *c = &g_cpu[i];
        if (!c->online) {
            printf("[ROCKY]  cpu%-d   OFFLINE\n", i);
            continue;
        }
        char load_s[8] = " N/A";
        if (c->util_pct >= 0)
            snprintf(load_s, sizeof(load_s), "%3d%%", c->util_pct);
        printf("[ROCKY]  cpu%-d   online   %7ld  %7ld  %7ld  %-14s  %s  %s\n",
               i,
               c->cur_khz > 0 ? c->cur_khz / 1000 : 0,
               c->max_khz > 0 ? c->max_khz / 1000 : 0,
               c->min_khz > 0 ? c->min_khz / 1000 : 0,
               c->governor,
               load_s,
               c->throttled ? "THROTTLED" : "ok");
    }

    printf("[ROCKY]  ────────────────────────────────────────────────────────\n");
    if (agg_pct >= 0)
        printf("[ROCKY]  Aggregate load: %d%%   Throttled cores: %d/%d\n",
               agg_pct, throttle_count, g_ncpus);
    else
        printf("[ROCKY]  Aggregate load: initialising...  Throttled cores: %d/%d\n",
               throttle_count, g_ncpus);

    /* APRIL: throttle */
    if (throttle_count > 0) {
        char ev[256];
        snprintf(ev, sizeof(ev),
                 "throttled_cores=%d total_cores=%d first_throttled_mhz=%ld",
                 throttle_count, g_ncpus,
                 g_cpu[0].cur_khz / 1000);
        splinterd_emit("cpu_throttle", ev);
    }

    /* APRIL: sustained high load */
    if (agg_pct >= LOAD_HIGH_PCT) {
        g_high_streak++;
        if (g_high_streak >= LOAD_HIGH_STREAK) {
            char ev[256];
            snprintf(ev, sizeof(ev),
                     "load_pct=%d streak_polls=%d throttled=%d",
                     agg_pct, g_high_streak, throttle_count);
            splinterd_emit("cpu_load_high", ev);
            g_high_streak = 0;  /* reset so we don't spam */
        }
    } else {
        g_high_streak = 0;
    }
}

/* ── Main ─────────────────────────────────────────────────────────────── */

int main(void)
{
    if (!daemon_core_init(DAEMON_NAME)) return 1;
    bexec_init();

    discover_cpus();
    printf("[ROCKY] Discovered %d CPU core(s)\n", g_ncpus);

    for (;;) {
        poll_cpu();
        sleep(POLL_SEC);
    }

    daemon_core_shutdown();
    return 0;
}
