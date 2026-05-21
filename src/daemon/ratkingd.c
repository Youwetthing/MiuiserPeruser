/*
 * ratkingd.c — Zombie Process & CPU/Memory Hog Hunter
 *
 * Every poll:
 *   - Walk /proc/[pid]/status to find zombie processes (State: Z)
 *   - Parse /proc/[pid]/stat to compute per-process CPU delta
 *   - Read /proc/[pid]/status VmRSS for top memory consumers
 *   - Report total process/thread count and system pressure
 *   - Emit APRIL events when zombies or runaway CPU hogs detected
 *
 * APRIL events emitted:
 *   zombie_detected  — one or more zombie processes present
 *   cpu_hog          — single process consuming > HOG_PCT of CPU
 *   mem_pressure     — available memory below MEM_LOW_MB
 */

#include "daemon_core.h"
#include <stdbool.h>
#include "ipc_globals.h"
volatile bool g_running = true;
#include "gaveld_emit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/un.h>

#define DAEMON_NAME   "ratkingd"
#define POLL_SEC      12
#define TOP_N         6
#define HOG_PCT       40    /* single-process CPU % = hog */
#define MEM_LOW_MB    200   /* MemAvailable below this → pressure event */

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

/* ── Process record ───────────────────────────────────────────────────── */

#define MAX_PROCS 512

typedef struct {
    int   pid;
    char  name[32];
    char  state;         /* R S D Z T … */
    int   uid;
    long  vmrss_kb;
    unsigned long long cpu_time;   /* utime + stime from /proc/pid/stat */
    int   cpu_pct;       /* computed across successive polls */
} proc_t;

static proc_t g_procs[MAX_PROCS];
static proc_t g_prev[MAX_PROCS];
static int    g_nprocs = 0, g_nprev = 0;
static int    g_first  = 1;

static long long g_prev_total_jiff = 0;   /* for CPU% normalisation */

/* ── /proc/[pid] readers ──────────────────────────────────────────────── */

static int read_status(int pid, proc_t *out)
{
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char line[256];
    out->pid = pid;
    out->state = '?';
    out->uid = -1;
    out->vmrss_kb = 0;
    out->name[0] = '\0';

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "Name:", 5) == 0) {
            char tmp[32] = {0};
            sscanf(line + 5, " %31s", tmp);
            strncpy(out->name, tmp, sizeof(out->name) - 1);
        } else if (strncmp(line, "State:", 6) == 0) {
            sscanf(line + 6, " %c", &out->state);
        } else if (strncmp(line, "Uid:", 4) == 0) {
            sscanf(line + 4, " %d", &out->uid);
        } else if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line + 6, " %ld", &out->vmrss_kb);
        }
    }
    fclose(f);
    return 1;
}

static unsigned long long read_cputime(int pid)
{
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    /* (1)pid (2)comm (3)state (4)ppid ... (14)utime (15)stime */
    unsigned long long utime = 0, stime = 0;
    unsigned long long dummy;
    char name[64]; char state;
    int ppid, pgrp, sess, tty, tpgid;
    unsigned flags;
    fscanf(f, "%*d %63s %c %d %d %d %d %d %u %llu %llu %llu %llu %llu %llu",
           name, &state, &ppid, &pgrp, &sess, &tty, &tpgid, &flags,
           &dummy, &dummy, &dummy, &dummy, &utime, &stime);
    fclose(f);
    return utime + stime;
}

/* ── /proc/meminfo ────────────────────────────────────────────────────── */

static void read_meminfo(long *total_kb, long *avail_kb, long *cached_kb)
{
    *total_kb = *avail_kb = *cached_kb = 0;
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "MemTotal:", 9) == 0)
            sscanf(line + 9, " %ld", total_kb);
        else if (strncmp(line, "MemAvailable:", 13) == 0)
            sscanf(line + 13, " %ld", avail_kb);
        else if (strncmp(line, "Cached:", 7) == 0 && line[7] == ' ')
            sscanf(line + 7, " %ld", cached_kb);
    }
    fclose(f);
}

/* ── Total jiffies from /proc/stat ───────────────────────────────────── */

static long long total_jiffies(void)
{
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

/* ── Poll ─────────────────────────────────────────────────────────────── */

static int cmp_vmrss(const void *a, const void *b)
{
    return (int)(((proc_t*)b)->vmrss_kb - ((proc_t*)a)->vmrss_kb);
}
static int cmp_cpu(const void *a, const void *b)
{
    return ((proc_t*)b)->cpu_pct - ((proc_t*)a)->cpu_pct;
}

static void poll_procs(void)
{
    DIR *d = opendir("/proc");
    if (!d) return;

    long long cur_total_j = total_jiffies();
    long long delta_j = (cur_total_j > g_prev_total_jiff)
                      ? cur_total_j - g_prev_total_jiff : 1;

    struct dirent *ent;
    g_nprocs = 0;
    int zombie_count = 0;
    int total_threads = 0;

    while ((ent = readdir(d)) != NULL && g_nprocs < MAX_PROCS) {
        /* Only numeric entries are PIDs */
        int is_pid = 1;
        for (char *c = ent->d_name; *c; c++)
            if (*c < '0' || *c > '9') { is_pid = 0; break; }
        if (!is_pid) continue;

        int pid = atoi(ent->d_name);
        proc_t *p = &g_procs[g_nprocs];
        if (!read_status(pid, p)) continue;
        p->cpu_time = read_cputime(pid);

        /* CPU% from delta vs previous snapshot */
        p->cpu_pct = 0;
        if (!g_first) {
            for (int j = 0; j < g_nprev; j++) {
                if (g_prev[j].pid == pid) {
                    unsigned long long dt =
                        (p->cpu_time > g_prev[j].cpu_time)
                        ? p->cpu_time - g_prev[j].cpu_time : 0;
                    p->cpu_pct = (int)(dt * 100ULL / (unsigned long long)delta_j);
                    break;
                }
            }
        }

        if (p->state == 'Z') zombie_count++;
        total_threads++;
        g_nprocs++;
    }
    closedir(d);

    /* Save for next round */
    memcpy(g_prev, g_procs, (size_t)g_nprocs * sizeof(proc_t));
    g_nprev = g_nprocs;
    g_prev_total_jiff = cur_total_j;
    g_first = 0;

    /* Memory */
    long total_kb, avail_kb, cached_kb;
    read_meminfo(&total_kb, &avail_kb, &cached_kb);
    long avail_mb = avail_kb / 1024;
    long total_mb = total_kb / 1024;
    int  mem_pct  = total_kb ? (int)((avail_kb * 100L) / total_kb) : 0;

    /* Sort copies for display */
    proc_t sorted_cpu[MAX_PROCS];
    proc_t sorted_mem[MAX_PROCS];
    memcpy(sorted_cpu, g_procs, (size_t)g_nprocs * sizeof(proc_t));
    memcpy(sorted_mem, g_procs, (size_t)g_nprocs * sizeof(proc_t));
    qsort(sorted_cpu, (size_t)g_nprocs, sizeof(proc_t), cmp_cpu);
    qsort(sorted_mem, (size_t)g_nprocs, sizeof(proc_t), cmp_vmrss);

    char ts[32];
    time_t t = time(NULL);
    strftime(ts, sizeof(ts), "%H:%M:%S", localtime(&t));

    printf("\n[RATKING] ── Process Report  %s ─────────────────────────────\n", ts);
    printf("[RATKING]  Processes: %-4d  Zombies: %-3d  "
           "Memory: %ldMB free / %ldMB total (%d%%)\n",
           g_nprocs, zombie_count, avail_mb, total_mb, mem_pct);

    /* Top CPU consumers */
    printf("[RATKING]\n[RATKING]  Top CPU consumers:\n");
    printf("[RATKING]  %-6s  %-20s  %s\n", "PID", "Name", "CPU%");
    int shown = 0;
    for (int i = 0; i < g_nprocs && shown < TOP_N; i++) {
        if (sorted_cpu[i].cpu_pct <= 0 && shown > 0) break;
        printf("[RATKING]  %-6d  %-20s  %d%%\n",
               sorted_cpu[i].pid, sorted_cpu[i].name, sorted_cpu[i].cpu_pct);
        shown++;
    }

    /* Top memory consumers */
    printf("[RATKING]\n[RATKING]  Top memory consumers:\n");
    printf("[RATKING]  %-6s  %-20s  %s\n", "PID", "Name", "RSS (MB)");
    shown = 0;
    for (int i = 0; i < g_nprocs && shown < TOP_N; i++) {
        if (sorted_mem[i].vmrss_kb <= 0) break;
        printf("[RATKING]  %-6d  %-20s  %.1f MB\n",
               sorted_mem[i].pid, sorted_mem[i].name,
               sorted_mem[i].vmrss_kb / 1024.0);
        shown++;
    }

    /* Zombie detail */
    if (zombie_count > 0) {
        printf("[RATKING]\n[RATKING]  ZOMBIE processes (State=Z):\n");
        for (int i = 0; i < g_nprocs; i++) {
            if (g_procs[i].state == 'Z')
                printf("[RATKING]  PID %-6d  %s\n",
                       g_procs[i].pid, g_procs[i].name);
        }
        char ev[256];
        snprintf(ev, sizeof(ev),
                 "zombie_count=%d total_procs=%d", zombie_count, g_nprocs);
        gaveld_emit(DAEMON_NAME, "ZOMBIE_DETECTED", (float)zombie_count, ev);
        splinterd_emit("zombie_detected", ev);
    }

    /* APRIL: CPU hog */
    if (sorted_cpu[0].cpu_pct >= HOG_PCT) {
        char ev[256];
        snprintf(ev, sizeof(ev),
                 "pid=%d name=%.24s cpu_pct=%d",
                 sorted_cpu[0].pid, sorted_cpu[0].name, sorted_cpu[0].cpu_pct);
        gaveld_emit(DAEMON_NAME, "CPU_HOG_PROCESS", (float)sorted_cpu[0].cpu_pct, ev);
        splinterd_emit("cpu_hog", ev);
        printf("[RATKING]  *** HOG: %s (pid %d) consuming %d%% CPU\n",
               sorted_cpu[0].name, sorted_cpu[0].pid, sorted_cpu[0].cpu_pct);
    }

    /* APRIL: memory pressure */
    if (avail_mb < MEM_LOW_MB && avail_mb > 0) {
        char ev[256];
        snprintf(ev, sizeof(ev),
                 "avail_mb=%ld total_mb=%ld pct=%d", avail_mb, total_mb, mem_pct);
        gaveld_emit(DAEMON_NAME, "MEM_PRESSURE", (float)avail_mb, ev);
        splinterd_emit("mem_pressure", ev);
        printf("[RATKING]  *** LOW MEMORY: %ldMB available\n", avail_mb);
    }
}

/* ── Main ─────────────────────────────────────────────────────────────── */

int main(void)
{
    if (!daemon_core_init(DAEMON_NAME)) return 1;

    for (;;) {
        poll_procs();
        printf("[RATKING]  Next scan in %ds\n", POLL_SEC);
        sleep(POLL_SEC);
    }

    daemon_core_shutdown();
    return 0;
}
