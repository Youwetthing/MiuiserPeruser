/*
 * fugitoidd.c — Foreground Activity Bridge & System Event Monitor
 *
 * Every poll:
 *   - Detect foreground app and activity from dumpsys window/activity
 *   - Scan logcat tail for crashes, ANRs, OOM kills
 *   - Count running services and background processes
 *   - Track app switches (new foreground = logged transition)
 *   - Read system memory/battery headline from /proc
 *   - Emit APRIL events on ANR, crash, or OOM kill
 *
 * APRIL events emitted:
 *   anr_detected     — Application Not Responding seen in logcat
 *   crash_detected   — FATAL EXCEPTION in logcat
 *   oom_kill         — lowmemorykiller event detected
 *   app_switch       — foreground app changed
 */

#include "daemon_core.h"
#include "ipc_globals.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>

#define DAEMON_NAME   "fugitoidd"
#define POLL_SEC      12
#define LOGCAT_LINES  30

static char g_prev_app[128] = {0};

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

/* ── Helpers ──────────────────────────────────────────────────────────── */

static char *run_cmd(const char *cmd)
{
    FILE *f = popen(cmd, "r");
    if (!f) return NULL;
    char *buf = malloc(16384);
    if (!buf) { pclose(f); return NULL; }
    size_t n = fread(buf, 1, 16383, f);
    buf[n] = '\0';
    pclose(f);
    return buf;
}

static int count_substr(const char *hay, const char *needle)
{
    if (!hay || !needle) return 0;
    int c = 0; const char *p = hay;
    while ((p = strstr(p, needle)) != NULL) { c++; p++; }
    return c;
}

/* Extract first line after keyword */
static int first_line_after(const char *hay, const char *kw,
                             char *out, size_t outlen)
{
    const char *p = strstr(hay, kw);
    if (!p) return 0;
    p += strlen(kw);
    while (*p == ' ' || *p == '=' || *p == ':') p++;
    size_t i = 0;
    while (*p && *p != '\n' && i < outlen - 1) out[i++] = *p++;
    out[i] = '\0';
    return i > 0;
}

/* ── Foreground activity detection ───────────────────────────────────── */

static void poll_foreground(void)
{
    char fg_app[128] = "unknown";
    char fg_activity[128] = "unknown";

    /* Method 1: dumpsys window — most reliable across MIUI versions */
    char *win = run_cmd(
        "dumpsys window windows 2>/dev/null | grep -E 'mCurrentFocus|mFocusedApp' | head -3");
    if (win) {
        /* mCurrentFocus=Window{... u0 com.pkg/com.pkg.Activity} */
        const char *p = strstr(win, "mCurrentFocus=");
        if (p) {
            p += 14;
            /* Skip "Window{... " to get to the package/activity */
            const char *brace = strstr(p, " u0 ");
            if (brace) {
                brace += 4;
                size_t i = 0;
                char tmp[128] = {0};
                while (*brace && *brace != '}' && *brace != ' ' && i < 127)
                    tmp[i++] = *brace++;
                tmp[i] = '\0';
                if (i > 0) {
                    /* tmp = "com.pkg/com.Activity" */
                    char *slash = strchr(tmp, '/');
                    if (slash) {
                        *slash = '\0';
                        strncpy(fg_app,      tmp,    sizeof(fg_app) - 1);
                        strncpy(fg_activity, slash+1, sizeof(fg_activity) - 1);
                    } else {
                        strncpy(fg_app, tmp, sizeof(fg_app) - 1);
                    }
                }
            }
        }
        free(win);
    }

    /* Method 2: dumpsys activity fallback */
    if (strcmp(fg_app, "unknown") == 0) {
        char *act = run_cmd(
            "dumpsys activity activities 2>/dev/null | grep -E 'mResumedActivity|Resumed' | head -2");
        if (act) {
            first_line_after(act, "mResumedActivity", fg_app, sizeof(fg_app));
            free(act);
        }
    }

    printf("[FUGITOID] Foreground : %s\n", fg_app);
    printf("[FUGITOID] Activity   : %s\n", fg_activity);

    /* Detect app switch */
    if (strcmp(fg_app, g_prev_app) != 0 && strcmp(fg_app, "unknown") != 0) {
        if (g_prev_app[0]) {
            printf("[FUGITOID] App switch : %s  →  %s\n", g_prev_app, fg_app);
            char ev[256];
            snprintf(ev, sizeof(ev),
                     "from=%.48s to=%.48s", g_prev_app, fg_app);
            splinterd_emit("app_switch", ev);
        }
        strncpy(g_prev_app, fg_app, sizeof(g_prev_app) - 1);
    }
}

/* ── Logcat crash / ANR / OOM audit ──────────────────────────────────── */

static void poll_logcat(void)
{
    char cmd[128];
    snprintf(cmd, sizeof(cmd),
             "logcat -d -t %d *:W 2>/dev/null", LOGCAT_LINES);
    char *log = run_cmd(cmd);
    if (!log) { printf("[FUGITOID] Logcat  : unavailable\n"); return; }

    int crashes = count_substr(log, "FATAL EXCEPTION");
    int anrs    = count_substr(log, "ANR in");
    int ooms    = count_substr(log, "lowmemorykiller") +
                  count_substr(log, "OOM killer");
    int wdogs   = count_substr(log, "watchdog");

    printf("[FUGITOID] Logcat   : crashes=%-3d  ANR=%-3d  OOM=%-3d  watchdog=%d\n",
           crashes, anrs, ooms, wdogs);

    if (anrs > 0) {
        /* Extract the ANR target package */
        char target[64] = "unknown";
        const char *p = strstr(log, "ANR in ");
        if (p) {
            p += 7;
            size_t i = 0;
            while (*p && *p != '\n' && *p != ' ' && i < 63) target[i++] = *p++;
            target[i] = '\0';
        }
        char ev[256];
        snprintf(ev, sizeof(ev), "count=%d target=%.48s", anrs, target);
        splinterd_emit("anr_detected", ev);
        printf("[FUGITOID] *** ANR: %s\n", target);
    }
    if (crashes > 0) {
        char target[64] = "unknown";
        const char *p = strstr(log, "FATAL EXCEPTION:");
        if (p) {
            p = strstr(p, "Process: ");
            if (p) {
                p += 9;
                size_t i = 0;
                while (*p && *p != '\n' && *p != ',' && i < 63) target[i++] = *p++;
                target[i] = '\0';
            }
        }
        char ev[256];
        snprintf(ev, sizeof(ev), "count=%d process=%.48s", crashes, target);
        splinterd_emit("crash_detected", ev);
        printf("[FUGITOID] *** CRASH: %s\n", target);
    }
    if (ooms > 0) {
        char ev[64];
        snprintf(ev, sizeof(ev), "oom_events=%d", ooms);
        splinterd_emit("oom_kill", ev);
        printf("[FUGITOID] *** OOM kill event(s): %d\n", ooms);
    }

    free(log);
}

/* ── Service + process counts ─────────────────────────────────────────── */

static void poll_services(void)
{
    char *svc = run_cmd(
        "dumpsys activity services 2>/dev/null | grep -c 'ServiceRecord{' 2>/dev/null");
    int nsvc = svc ? atoi(svc) : -1;
    if (svc) free(svc);

    char *procs = run_cmd(
        "dumpsys activity processes 2>/dev/null | grep -c 'ProcessRecord{' 2>/dev/null");
    int nproc = procs ? atoi(procs) : -1;
    if (procs) free(procs);

    printf("[FUGITOID] Services : %d running\n", nsvc < 0 ? 0 : nsvc);
    printf("[FUGITOID] Processes: %d tracked by ActivityManager\n",
           nproc < 0 ? 0 : nproc);
}

/* ── System headline ──────────────────────────────────────────────────── */

static void poll_system_headline(void)
{
    /* Battery level from /sys (quick, no dumpsys needed) */
    FILE *f;
    int bat_cap = -1;
    char bat_status[32] = "unknown";

    f = fopen("/sys/class/power_supply/battery/capacity", "r");
    if (f) { fscanf(f, "%d", &bat_cap); fclose(f); }

    f = fopen("/sys/class/power_supply/battery/status", "r");
    if (f) {
        fgets(bat_status, sizeof(bat_status), f);
        bat_status[strcspn(bat_status, "\n")] = '\0';
        fclose(f);
    }

    /* MemAvailable */
    long avail_mb = 0;
    f = fopen("/proc/meminfo", "r");
    if (f) {
        char line[128]; long v;
        while (fgets(line, sizeof(line), f)) {
            if (sscanf(line, "MemAvailable: %ld", &v) == 1)
                { avail_mb = v / 1024; break; }
        }
        fclose(f);
    }

    printf("[FUGITOID] Battery  : %d%%  (%s)\n", bat_cap, bat_status);
    printf("[FUGITOID] MemFree  : %ldMB available\n", avail_mb);
}

/* ── Main ─────────────────────────────────────────────────────────────── */

int main(void)
{
    if (!daemon_core_init(DAEMON_NAME)) return 1;

    int cycle = 0;
    char ts[32];

    for (;;) {
        time_t t = time(NULL);
        strftime(ts, sizeof(ts), "%H:%M:%S", localtime(&t));
        printf("\n[FUGITOID] ── System Bridge  %s  (poll #%d) ──────────────\n",
               ts, ++cycle);

        poll_foreground();
        poll_logcat();
        poll_services();
        poll_system_headline();

        printf("[FUGITOID] Next poll in %ds\n", POLL_SEC);
        sleep(POLL_SEC);
    }

    daemon_core_shutdown();
    return 0;
}
