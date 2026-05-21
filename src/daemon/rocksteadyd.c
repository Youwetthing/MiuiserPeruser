/*
 * rocksteadyd.c — CPU Load & Frequency Monitor
 *
 * Domain: CPU muscle on MTK HyperOS
 *   - Per-core frequency: /sys/devices/system/cpu/cpuN/cpufreq/
 *   - CPU load: /proc/stat delta between polls
 *   - Governor per cluster
 *   - MTK topology: cpu0-3 = efficiency, cpu4-7 = performance
 *
 * Signals emitted:
 *   CPU_HOG              — single process > HOG_PCT of total CPU
 *   CPU_HOG_CRITICAL     — single process > CRIT_PCT
 *   CPU_CLUSTER_IMBALANCE— efficiency cluster busier than perf cluster
 *   GOVERNOR_PERFORMANCE — any core locked to "performance" governor
 *   ALL_CORES_MAXED      — all cores at 100% max freq sustained
 *
 * IPC (turtlecom worker):
 *   CAPABILITY?      → CAPABILITY CPU FREQ
 *                       CAPABILITY CPU LOAD
 *                       CAPABILITY CPU CLUSTER
 *                       CAPABILITY CPU GOV
 *   CPU FREQ         → per-core cur/max/ratio table
 *   CPU LOAD         → overall CPU % (from /proc/stat)
 *   CPU CLUSTER      → efficiency vs performance cluster summary
 *   CPU GOV          → governor per cluster
 *   HEARTBEAT SEND   → HEARTBEAT ROCKSTEADY
 */

#include "daemon_core.h"
#include "gaveld_emit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
volatile sig_atomic_t g_running = 1;

#define DAEMON_NAME "rocksteadyd"
#define BUS_PATH    "/data/data/com.termux/files/home/MiuiserPeruser/pipes/turtlecom.sock"

#define NUM_CORES    8
#define HOG_PCT     40.0f   /* % total CPU → CPU_HOG */
#define CRIT_PCT    70.0f   /* % total CPU → CPU_HOG_CRITICAL */

/* Probe every ~15 seconds */
#define PROBE_TICKS 150
/* Heartbeat every ~30s */
#define HB_TICKS    300

/* ── IPC ──────────────────────────────────────────────────────────────── */

static int connect_bus(void)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, BUS_PATH, sizeof(addr.sun_path) - 1);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    fcntl(fd, F_SETFL, O_NONBLOCK);
    return fd;
}

#ifndef SPLINTER_SOCKET
#define SPLINTER_SOCKET "/data/data/com.termux/files/home/MiuiserPeruser/pipes/splinterd.sock"
#endif

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

/* ── CPU frequency ────────────────────────────────────────────────────── */

typedef struct {
    long cur_khz;
    long max_khz;
    char governor[32];
    int  online;
} core_info_t;

static core_info_t g_cores[NUM_CORES];

static void read_cpu_freq(void)
{
    for (int cpu = 0; cpu < NUM_CORES; cpu++) {
        core_info_t *c = &g_cores[cpu];
        char path[128];
        FILE *f;

        /* Online check */
        snprintf(path, sizeof(path),
                 "/sys/devices/system/cpu/cpu%d/online", cpu);
        f = fopen(path, "r");
        c->online = 1;   /* cpu0 has no online file, always online */
        if (f) { fscanf(f, "%d", &c->online); fclose(f); }

        if (!c->online) { c->cur_khz = 0; c->max_khz = 0; continue; }

        /* Current frequency */
        snprintf(path, sizeof(path),
                 "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", cpu);
        f = fopen(path, "r"); c->cur_khz = 0;
        if (f) { fscanf(f, "%ld", &c->cur_khz); fclose(f); }

        /* Hardware max */
        snprintf(path, sizeof(path),
                 "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", cpu);
        f = fopen(path, "r"); c->max_khz = 0;
        if (f) { fscanf(f, "%ld", &c->max_khz); fclose(f); }

        /* Governor */
        snprintf(path, sizeof(path),
                 "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor", cpu);
        f = fopen(path, "r"); c->governor[0] = '\0';
        if (f) {
            fgets(c->governor, sizeof(c->governor), f);
            c->governor[strcspn(c->governor, "\n\r")] = '\0';
            fclose(f);
        }
        if (!c->governor[0]) strncpy(c->governor, "unknown", sizeof(c->governor));
    }
}

/* ── CPU load from /proc/stat ─────────────────────────────────────────── */

typedef struct {
    unsigned long long user, nice, system, idle, iowait, irq, softirq;
} cpu_stat_t;

static cpu_stat_t g_prev_stat;
static int        g_first_stat = 1;

static int read_proc_stat(cpu_stat_t *s)
{
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return 0;
    /* First line: "cpu  user nice system idle iowait irq softirq ..." */
    int ok = (fscanf(f, "cpu %llu %llu %llu %llu %llu %llu %llu",
                     &s->user, &s->nice, &s->system, &s->idle,
                     &s->iowait, &s->irq, &s->softirq) == 7);
    fclose(f);
    return ok;
}

/* Returns overall CPU busy % since last call. -1 if first call. */
static float cpu_busy_percent(void)
{
    cpu_stat_t cur;
    if (!read_proc_stat(&cur)) return -1.0f;

    if (g_first_stat) {
        g_prev_stat  = cur;
        g_first_stat = 0;
        return -1.0f;
    }

    unsigned long long d_user    = cur.user    - g_prev_stat.user;
    unsigned long long d_nice    = cur.nice    - g_prev_stat.nice;
    unsigned long long d_system  = cur.system  - g_prev_stat.system;
    unsigned long long d_idle    = cur.idle    - g_prev_stat.idle;
    unsigned long long d_iowait  = cur.iowait  - g_prev_stat.iowait;
    unsigned long long d_irq     = cur.irq     - g_prev_stat.irq;
    unsigned long long d_softirq = cur.softirq - g_prev_stat.softirq;

    unsigned long long busy  = d_user + d_nice + d_system + d_irq + d_softirq;
    unsigned long long total = busy + d_idle + d_iowait;

    g_prev_stat = cur;
    if (!total) return 0.0f;
    return (float)(busy * 100ULL) / (float)total;
}

/* ── Top hog process ──────────────────────────────────────────────────── */

static char g_hog_proc[128] = {0};
static float g_hog_pct = 0.0f;

static void find_hog_process(void)
{
    /* top -bn1: first data line after header is highest CPU consumer */
    FILE *f = popen("top -bn1 -o %CPU 2>/dev/null | awk 'NR>7 && NF>0 {print $9,$12; exit}'", "r");
    if (!f) return;
    char cpu_str[16] = {0};
    char name[96]    = {0};
    if (fscanf(f, "%15s %95s", cpu_str, name) == 2) {
        g_hog_pct = atof(cpu_str);
        strncpy(g_hog_proc, name, sizeof(g_hog_proc) - 1);
    }
    pclose(f);
}

/* ── Cluster analysis ─────────────────────────────────────────────────── */

typedef struct {
    long avg_cur_khz;
    long max_khz;
    float util;   /* cur/max */
    char governor[32];
} cluster_t;

static void cluster_stats(int start, int end, cluster_t *out)
{
    long sum_cur = 0, sum_max = 0;
    int  count   = 0;
    out->governor[0] = '\0';

    for (int i = start; i <= end; i++) {
        if (!g_cores[i].online) continue;
        sum_cur += g_cores[i].cur_khz;
        sum_max += g_cores[i].max_khz;
        count++;
        if (!out->governor[0])
            strncpy(out->governor, g_cores[i].governor, sizeof(out->governor) - 1);
    }
    if (!count) { out->avg_cur_khz = 0; out->max_khz = 0; out->util = 0.0f; return; }
    out->avg_cur_khz = sum_cur / count;
    out->max_khz     = sum_max / count;
    out->util        = out->max_khz ? (float)out->avg_cur_khz / (float)out->max_khz : 0.0f;
}

/* ── Probe & emit ─────────────────────────────────────────────────────── */

static void probe_and_emit(int ipc_fd, const char *cmd)
{
    read_cpu_freq();
    float overall_pct = cpu_busy_percent();
    find_hog_process();

    cluster_t eff, perf;
    cluster_stats(0, 3, &eff);
    cluster_stats(4, 7, &perf);

    /* ── Log ──────────────────────────────────────────────────────── */
    daemon_log_info("CPU: overall=%.1f%% eff_util=%.0f%% perf_util=%.0f%% hog=%s(%.1f%%)",
                    overall_pct < 0 ? 0.0f : overall_pct,
                    eff.util  * 100.0f,
                    perf.util * 100.0f,
                    g_hog_proc[0] ? g_hog_proc : "none",
                    g_hog_pct);

    /* ── IPC response ─────────────────────────────────────────────── */
    if (ipc_fd >= 0 && cmd) {
        if (strncmp(cmd, "CPU FREQ", 8) == 0) {
            for (int i = 0; i < NUM_CORES; i++) {
                dprintf(ipc_fd,
                        "CPU FREQ core=%d cur=%ldkHz max=%ldkHz ratio=%.0f%% gov=%s\n",
                        i,
                        g_cores[i].cur_khz, g_cores[i].max_khz,
                        g_cores[i].max_khz ?
                            (float)g_cores[i].cur_khz / (float)g_cores[i].max_khz * 100.0f : 0.0f,
                        g_cores[i].governor);
            }
        } else if (strncmp(cmd, "CPU LOAD", 8) == 0) {
            dprintf(ipc_fd, "CPU LOAD overall=%.1f%% hog=%s(%.1f%%)\n",
                    overall_pct < 0 ? 0.0f : overall_pct,
                    g_hog_proc[0] ? g_hog_proc : "none",
                    g_hog_pct);
        } else if (strncmp(cmd, "CPU CLUSTER", 11) == 0) {
            dprintf(ipc_fd,
                    "CPU CLUSTER eff_avg=%ldkHz(%.0f%%) perf_avg=%ldkHz(%.0f%%)\n",
                    eff.avg_cur_khz,  eff.util  * 100.0f,
                    perf.avg_cur_khz, perf.util * 100.0f);
        } else if (strncmp(cmd, "CPU GOV", 7) == 0) {
            dprintf(ipc_fd, "CPU GOV eff=%s perf=%s\n",
                    eff.governor[0]  ? eff.governor  : "unknown",
                    perf.governor[0] ? perf.governor : "unknown");
        }
    }

    /* ── Signal evaluation ─────────────────────────────────────────── */

    /* CPU hog */
    if (g_hog_pct >= CRIT_PCT) {
        char ctx[128];
        snprintf(ctx, sizeof(ctx), "proc=%s pct=%.1f",
                 g_hog_proc[0] ? g_hog_proc : "unknown", g_hog_pct);
        gaveld_emit(DAEMON_NAME, "CPU_HOG_CRITICAL", g_hog_pct, ctx);
        splinterd_emit("cpu_hog_critical", ctx);
    } else if (g_hog_pct >= HOG_PCT) {
        char ctx[128];
        snprintf(ctx, sizeof(ctx), "proc=%s pct=%.1f",
                 g_hog_proc[0] ? g_hog_proc : "unknown", g_hog_pct);
        gaveld_emit(DAEMON_NAME, "CPU_HOG", g_hog_pct, ctx);
        splinterd_emit("cpu_hog", ctx);
    }

    /* Cluster imbalance: efficiency cluster running harder than performance */
    if (eff.util > 0.1f && perf.util > 0.1f && eff.util > perf.util + 0.20f) {
        char ctx[128];
        snprintf(ctx, sizeof(ctx),
                 "eff_util=%.0f%% perf_util=%.0f%%",
                 eff.util * 100.0f, perf.util * 100.0f);
        gaveld_emit(DAEMON_NAME, "CPU_CLUSTER_IMBALANCE", eff.util * 100.0f, ctx);
        splinterd_emit("cpu_cluster_imbalance", ctx);
    }

    /* Governor locked to performance */
    int gov_perf = (strcmp(eff.governor,  "performance") == 0 ||
                    strcmp(perf.governor, "performance") == 0);
    if (gov_perf) {
        char ctx[128];
        snprintf(ctx, sizeof(ctx), "eff_gov=%s perf_gov=%s",
                 eff.governor, perf.governor);
        gaveld_emit(DAEMON_NAME, "GOVERNOR_PERFORMANCE", 0.0f, ctx);
        splinterd_emit("governor_performance", ctx);
    }

    /* All cores maxed: both clusters at ≥ 95% utilisation */
    if (eff.util >= 0.95f && perf.util >= 0.95f) {
        char ctx[64];
        snprintf(ctx, sizeof(ctx),
                 "eff=%.0f%% perf=%.0f%%",
                 eff.util * 100.0f, perf.util * 100.0f);
        gaveld_emit(DAEMON_NAME, "ALL_CORES_MAXED", 100.0f, ctx);
        splinterd_emit("all_cores_maxed", ctx);
    }
}

/* ── Main ─────────────────────────────────────────────────────────────── */

int main(void)
{
    

    int fd = connect_bus();
    if (fd < 0) {
        daemon_log_error(DAEMON_NAME ": cannot connect to turtlecom — exiting");
        return 1;
    }

    write(fd, "HELLO WORKER ROCKSTEADY\n", 24);
    daemon_log_info(DAEMON_NAME " ONLINE — CPU Load & Frequency Monitor");

    char buf[256];
    int  tick = 0, hb_tick = 0;

    for (;;) {
        usleep(100000);   /* 100ms */
        tick++;
        hb_tick++;

        /* Heartbeat */
        if (hb_tick >= HB_TICKS) {
            write(fd, "HEARTBEAT ROCKSTEADY\n", 21);
            hb_tick = 0;
        }

        /* Periodic probe */
        if (tick >= PROBE_TICKS) {
            tick = 0;
            probe_and_emit(-1, NULL);
        }

        /* IPC */
        int n = read(fd, buf, sizeof(buf) - 1);
        if (n <= 0) continue;
        buf[n] = '\0';

        if (strncmp(buf, "CAPABILITY?", 11) == 0) {
            write(fd, "CAPABILITY CPU FREQ\n",    20);
            write(fd, "CAPABILITY CPU LOAD\n",    20);
            write(fd, "CAPABILITY CPU CLUSTER\n", 23);
            write(fd, "CAPABILITY CPU GOV\n",     19);
            write(fd, "CAPABILITY HEARTBEAT SEND\n", 26);
            continue;
        }

        if (strncmp(buf, "HEARTBEAT SEND", 14) == 0) {
            write(fd, "HEARTBEAT ROCKSTEADY\n", 21);
            continue;
        }

        if (strncmp(buf, "CPU", 3) == 0) {
            probe_and_emit(fd, buf);
            continue;
        }
    }

    return 0;
}
