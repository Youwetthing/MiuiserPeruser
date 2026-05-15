/*
 * ratkingd.c — Zombie Process & CPU/Memory Hog Hunter
 *
 * Every poll:
 *   - CPU usage via 'dumpsys cpuinfo' (system-wide, all apps, kernel-reported %)
 *   - Full process list + zombie detection via 'ps -A' through privileged backend
 *   - Top memory consumers parsed from ps -A RSS column
 *   - /proc/meminfo for available memory
 *   - APRIL events emitted on zombies, CPU hogs, memory pressure
 *
 * APRIL events emitted:
 *   zombie_detected  — one or more zombie processes present
 *   cpu_hog          — single process consuming > HOG_PCT of CPU
 *   mem_pressure     — available memory below MEM_LOW_MB
 */

#include "daemon_core.h"
#include "ipc_globals.h"
#include "backend_exec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>

#define DAEMON_NAME       "ratkingd"
#define POLL_SEC          12
#define TOP_N             6
#define HOG_PCT           40
#define MEM_LOW_MB        200
#define MAX_ENTRIES       64
#define UID_MAP_PATH      BASE "/data/uid_package_map.txt"
#define MAX_UID_ENTRIES   1024

/* ── UID → package map ────────────────────────────────────────────────── */

typedef struct { int uid; char pkg[80]; } uid_entry_t;
static uid_entry_t g_uid_map[MAX_UID_ENTRIES];
static int         g_uid_map_n = 0;

static void load_uid_map(void)
{
    char *raw = bexec_read_file(UID_MAP_PATH);
    if (!raw) return;
    const char *p = raw;
    while (*p && g_uid_map_n < MAX_UID_ENTRIES) {
        int uid = 0;
        char pkg[80] = {0};
        if (sscanf(p, "%d %79s", &uid, pkg) == 2) {
            g_uid_map[g_uid_map_n].uid = uid;
            strncpy(g_uid_map[g_uid_map_n].pkg, pkg,
                    sizeof(g_uid_map[0].pkg) - 1);
            g_uid_map_n++;
        }
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
    }
    free(raw);
    printf("[RATKING] Loaded %d UID→package entries\n", g_uid_map_n);
}

/* Resolve "u0_a311" → com.termux using the map */
static const char *uid_to_pkg(const char *user_str)
{
    /* u0_aXXX → UID = 10000 + XXX */
    if (strncmp(user_str, "u0_a", 4) == 0) {
        int sub = atoi(user_str + 4);
        int uid = 10000 + sub;
        for (int i = 0; i < g_uid_map_n; i++)
            if (g_uid_map[i].uid == uid)
                return g_uid_map[i].pkg;
    }
    return NULL;
}

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

/* ── Lightweight process record ───────────────────────────────────────── */

typedef struct {
    int  pid;
    char name[64];
    int  val;   /* cpu_pct OR rss_kb depending on context */
} pentry_t;

/* ── /proc/meminfo (direct, world-readable on Android) ───────────────── */

static void read_meminfo(long *total_kb, long *avail_kb)
{
    *total_kb = *avail_kb = 0;
    char *raw = bexec_read_file("/proc/meminfo");
    if (!raw) return;
    char *p = raw;
    while (*p) {
        long v = 0;
        if (sscanf(p, "MemTotal: %ld", &v) == 1)      *total_kb = v;
        else if (sscanf(p, "MemAvailable: %ld", &v) == 1) *avail_kb = v;
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
    }
    free(raw);
}

/* ── Parse 'dumpsys cpuinfo' ──────────────────────────────────────────── *
 *
 * Relevant lines:
 *   "  37% 1234/com.android.systemui: 30% user + 7% kernel"
 *   "40% TOTAL: 33% user + 7% kernel + 0% iowait"
 *
 * Returns number of process entries found; sets *total_pct if TOTAL line seen.
 * ─────────────────────────────────────────────────────────────────────── */

static int parse_cpuinfo(const char *dump, pentry_t *out, int maxn, int *total_pct)
{
    if (!dump) return 0;
    *total_pct = -1;
    int n = 0;
    const char *p = dump;

    while (*p) {
        /* Advance to next non-blank line */
        while (*p == '\n' || *p == '\r') p++;
        if (!*p) break;

        const char *eol = strchr(p, '\n');
        size_t llen = eol ? (size_t)(eol - p) : strlen(p);
        char line[256];
        if (llen >= sizeof(line)) llen = sizeof(line) - 1;
        memcpy(line, p, llen);
        line[llen] = '\0';
        p = eol ? eol + 1 : p + llen;

        /* "40% TOTAL: …" */
        if (strstr(line, "TOTAL:")) {
            int tp = 0;
            if (sscanf(line, " %d%% TOTAL", &tp) == 1)
                *total_pct = tp;
            continue;
        }

        /* "  37% 1234/com.package: …" */
        int pct = 0, pid = 0;
        char name[64] = {0};
        if (sscanf(line, " %d%% %d/%63[^:]", &pct, &pid, name) == 3
            && n < maxn)
        {
            out[n].pid = pid;
            out[n].val = pct;
            strncpy(out[n].name, name, sizeof(out[n].name) - 1);
            n++;
            continue;
        }

        /* Alternate: "  37% 1234: com.package (in some MIUI builds) */
        if (sscanf(line, " %d%% %d: %63[^\n]", &pct, &pid, name) == 3
            && n < maxn)
        {
            out[n].pid = pid;
            out[n].val = pct;
            strncpy(out[n].name, name, sizeof(out[n].name) - 1);
            n++;
        }
    }
    return n;
}

/* ── Parse 'ps -A' output ─────────────────────────────────────────────── *
 *
 * Android toybox ps -A columns (typical):
 *   USER  PID  PPID  VSZ  RSS  WCHAN  ADDR  S  NAME
 *
 * Returns total process count; fills zombie_count; top memory entries
 * sorted descending.
 * ─────────────────────────────────────────────────────────────────────── */

static int cmp_rss_desc(const void *a, const void *b)
{
    return ((pentry_t*)b)->val - ((pentry_t*)a)->val;
}

static int parse_ps(const char *ps_out, int *zombie_count,
                    pentry_t *mem_top, int maxn)
{
    *zombie_count = 0;
    int nm = 0;
    int total = 0;
    if (!ps_out) return 0;

    const char *p = ps_out;
    int header_seen = 0;

    while (*p) {
        while (*p == '\n' || *p == '\r') p++;
        if (!*p) break;
        const char *eol = strchr(p, '\n');
        size_t llen = eol ? (size_t)(eol - p) : strlen(p);
        char line[512];
        if (llen >= sizeof(line)) llen = sizeof(line) - 1;
        memcpy(line, p, llen);
        line[llen] = '\0';
        p = eol ? eol + 1 : p + llen;

        if (!header_seen) { header_seen = 1; continue; }
        if (!line[0] || line[0] == '\r') continue;

        total++;

        /* Columns: USER PID PPID VSZ RSS WCHAN ADDR S NAME */
        char user[32], wchan[32], addr[32], state[4], name[64];
        int  pid = 0, ppid = 0;
        long vsz = 0, rss = 0;

        if (sscanf(line, "%31s %d %d %ld %ld %31s %31s %3s %63[^\n]",
                   user, &pid, &ppid, &vsz, &rss, wchan, addr, state, name) >= 8)
        {
            if (state[0] == 'Z') (*zombie_count)++;

            if (nm < maxn) {
                mem_top[nm].pid = pid;
                mem_top[nm].val = (int)(rss); /* rss in KB */
                /* name might have trailing spaces */
                char *sp = strchr(name, ' ');
                if (sp) *sp = '\0';
                /* Resolve u0_aXXX → package name via UID map */
                const char *resolved = uid_to_pkg(user);
                if (resolved && strncmp(name, "app_process", 11) == 0)
                    strncpy(mem_top[nm].name, resolved,
                            sizeof(mem_top[nm].name) - 1);
                else
                    strncpy(mem_top[nm].name, name,
                            sizeof(mem_top[nm].name) - 1);
                nm++;
            }
        }
    }

    if (nm > 1)
        qsort(mem_top, (size_t)nm, sizeof(pentry_t), cmp_rss_desc);

    return total;
}

/* ── Poll ─────────────────────────────────────────────────────────────── */

static int g_cycle = 0;

static void poll_procs(void)
{
    char ts[32];
    time_t t = time(NULL);
    strftime(ts, sizeof(ts), "%H:%M:%S", localtime(&t));

    /* ── CPU: dumpsys cpuinfo (all procs, kernel-reported %) ─────────── */
    char *cpudump = bexec("dumpsys cpuinfo 2>/dev/null");
    pentry_t cpu_top[MAX_ENTRIES];
    int ncpu = 0, total_cpu_pct = -1;
    if (cpudump) {
        ncpu = parse_cpuinfo(cpudump, cpu_top, MAX_ENTRIES, &total_cpu_pct);
        free(cpudump);
    }

    /* ── Process list: ps -A (all procs via rish) ─────────────────────── */
    char *ps_out = bexec("ps -A 2>/dev/null");
    pentry_t mem_top[MAX_ENTRIES];
    int zombie_count = 0;
    int total_procs = parse_ps(ps_out, &zombie_count, mem_top, MAX_ENTRIES);
    if (ps_out) free(ps_out);

    /* ── Memory: /proc/meminfo ────────────────────────────────────────── */
    long total_kb = 0, avail_kb = 0;
    read_meminfo(&total_kb, &avail_kb);
    long avail_mb = avail_kb / 1024;
    long total_mb = total_kb / 1024;
    int  mem_pct  = total_kb ? (int)(avail_kb * 100L / total_kb) : 0;

    /* ── Print ────────────────────────────────────────────────────────── */
    printf("\n[RATKING] ── Process Report  %s  (poll #%d) ─────────────────\n",
           ts, ++g_cycle);

    printf("[RATKING]  Processes: %-4d  Zombies: %-3d  "
           "Memory: %ldMB free / %ldMB total (%d%%)\n",
           total_procs, zombie_count, avail_mb, total_mb, mem_pct);

    if (total_cpu_pct >= 0)
        printf("[RATKING]  System CPU total: %d%%\n", total_cpu_pct);

    /* Top CPU */
    printf("[RATKING]\n[RATKING]  Top CPU consumers  (via dumpsys cpuinfo):\n");
    printf("[RATKING]  %-6s  %-38s  %s\n", "PID", "Name/Package", "CPU%");
    int shown = 0;
    for (int i = 0; i < ncpu && shown < TOP_N; i++, shown++)
        printf("[RATKING]  %-6d  %-38s  %d%%\n",
               cpu_top[i].pid, cpu_top[i].name, cpu_top[i].val);
    if (!ncpu)
        printf("[RATKING]  (dumpsys cpuinfo unavailable — check rish)\n");

    /* Top Memory */
    printf("[RATKING]\n[RATKING]  Top memory consumers  (via ps -A RSS):\n");
    printf("[RATKING]  %-6s  %-38s  %s\n", "PID", "Name", "RSS (MB)");
    shown = 0;
    for (int i = 0; i < MAX_ENTRIES && shown < TOP_N; i++) {
        if (!mem_top[i].val) break;
        printf("[RATKING]  %-6d  %-38s  %.1f MB\n",
               mem_top[i].pid, mem_top[i].name, mem_top[i].val / 1024.0);
        shown++;
    }

    /* Zombies */
    if (zombie_count > 0) {
        printf("[RATKING]\n[RATKING]  *** ZOMBIES: %d processes in state Z\n",
               zombie_count);
        char ev[128];
        snprintf(ev, sizeof(ev),
                 "zombie_count=%d total_procs=%d", zombie_count, total_procs);
        splinterd_emit("zombie_detected", ev);
    }

    /* CPU hog */
    if (ncpu > 0 && cpu_top[0].val >= HOG_PCT) {
        char ev[256];
        snprintf(ev, sizeof(ev), "pid=%d name=%.36s cpu_pct=%d",
                 cpu_top[0].pid, cpu_top[0].name, cpu_top[0].val);
        splinterd_emit("cpu_hog", ev);
        printf("[RATKING]  *** HOG: %s (pid %d)  %d%% CPU\n",
               cpu_top[0].name, cpu_top[0].pid, cpu_top[0].val);
    }

    /* Memory pressure */
    if (avail_mb > 0 && avail_mb < MEM_LOW_MB) {
        char ev[128];
        snprintf(ev, sizeof(ev),
                 "avail_mb=%ld total_mb=%ld pct=%d", avail_mb, total_mb, mem_pct);
        splinterd_emit("mem_pressure", ev);
        printf("[RATKING]  *** LOW MEMORY: %ldMB available\n", avail_mb);
    }
}

/* ── Main ─────────────────────────────────────────────────────────────── */

int main(void)
{
    if (!daemon_core_init(DAEMON_NAME)) return 1;
    bexec_init();
    load_uid_map();

    for (;;) {
        poll_procs();
        printf("[RATKING]  Next scan in %ds\n", POLL_SEC);
        sleep(POLL_SEC);
    }

    daemon_core_shutdown();
    return 0;
}
