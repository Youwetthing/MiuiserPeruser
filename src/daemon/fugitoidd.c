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
#include "backend_exec.h"
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

static char *run_cmd(const char *cmd) { return bexec_n(cmd, 32768); }

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

/* Extract "com.package/com.Activity" from a Window{...} token.
 * Handles: "u0 com.pkg/Act}", "u0 com.pkg}", space-delimited variants. */
static int extract_window_pkg(const char *token, char *pkg, char *act,
                               size_t plen, size_t alen)
{
    /* Skip past any "u0 ", "u0_a123 ", or "0 " prefix */
    const char *p = token;
    while (*p == ' ') p++;
    /* skip the "uN " or "uN_aM " user tag */
    if (*p == 'u') {
        while (*p && *p != ' ') p++;
        while (*p == ' ') p++;
    }
    /* p now points at "com.pkg/com.Activity}" or "com.pkg}" */
    char buf[128] = {0};
    size_t i = 0;
    while (*p && *p != '}' && *p != ' ' && i < sizeof(buf) - 1)
        buf[i++] = *p++;
    buf[i] = '\0';
    if (!i || !strchr(buf, '.')) return 0;  /* must look like a package */

    char *slash = strchr(buf, '/');
    if (slash) {
        size_t ppart = (size_t)(slash - buf);
        if (ppart >= plen) ppart = plen - 1;
        memcpy(pkg, buf, ppart); pkg[ppart] = '\0';
        strncpy(act, slash + 1, alen - 1);
    } else {
        strncpy(pkg, buf, plen - 1);
        strncpy(act, "(unknown)", alen - 1);
    }
    return 1;
}

static void poll_foreground(void)
{
    char fg_app[128] = "unknown";
    char fg_activity[128] = "unknown";

    /* Method 1: dumpsys window — parse mCurrentFocus window token.
     * The Window{} token format is vendor-dependent; we search for
     * the last space-delimited token before '}' that contains '.' */
    char *win = run_cmd(
        "dumpsys window 2>/dev/null | grep -E 'mCurrentFocus|mFocusedApp' | head -3");
    if (win) {
        const char *p = win;
        /* Try mCurrentFocus= */
        const char *fc = strstr(p, "mCurrentFocus=");
        if (fc) {
            /* Find the Window{} block content */
            const char *ob = strchr(fc, '{');
            const char *cb = ob ? strchr(ob, '}') : NULL;
            if (ob && cb) {
                /* Extract last space-delimited token before '}' */
                char block[256] = {0};
                size_t blen = (size_t)(cb - ob - 1);
                if (blen >= sizeof(block)) blen = sizeof(block) - 1;
                memcpy(block, ob + 1, blen);
                block[blen] = '\0';
                extract_window_pkg(block, fg_app, fg_activity,
                                   sizeof(fg_app), sizeof(fg_activity));
            }
        }
        free(win);
    }

    /* Method 2: dumpsys activity top — gives foreground stack directly */
    if (strcmp(fg_app, "unknown") == 0) {
        char *top = run_cmd(
            "dumpsys activity top 2>/dev/null | grep -E 'ACTIVITY|Resumed' | head -3");
        if (top) {
            /* "  ACTIVITY com.package/com.Activity pid=1234" */
            const char *at = strstr(top, "ACTIVITY ");
            if (at) {
                at += 9;
                char tmp[128] = {0};
                size_t i = 0;
                while (*at && *at != ' ' && *at != '\n' && i < 127)
                    tmp[i++] = *at++;
                tmp[i] = '\0';
                if (i > 0) {
                    char *slash = strchr(tmp, '/');
                    if (slash) {
                        *slash = '\0';
                        strncpy(fg_app,      tmp,     sizeof(fg_app) - 1);
                        strncpy(fg_activity, slash+1, sizeof(fg_activity) - 1);
                    } else {
                        strncpy(fg_app, tmp, sizeof(fg_app) - 1);
                    }
                }
            }
            free(top);
        }
    }

    /* Method 3: mResumedActivity */
    if (strcmp(fg_app, "unknown") == 0) {
        char *act = run_cmd(
            "dumpsys activity activities 2>/dev/null | "
            "grep -E 'mResumedActivity|mCurrentFocus' | head -2");
        if (act) {
            /* "mResumedActivity: ActivityRecord{... com.pkg/.Activity ...}" */
            const char *mr = strstr(act, "ActivityRecord{");
            if (!mr) mr = strstr(act, "mResumedActivity=");
            if (mr) {
                char buf[128] = {0};
                first_line_after(mr, "ActivityRecord{", buf, sizeof(buf));
                if (buf[0]) {
                    /* skip leading tokens to find package/activity */
                    char *sp = strstr(buf, " ");
                    if (sp) {
                        sp++;
                        sp = strstr(sp, " ");
                        if (sp) {
                            sp++;
                            char *slash = strchr(sp, '/');
                            if (slash) {
                                size_t plen = (size_t)(slash - sp);
                                if (plen < sizeof(fg_app)) {
                                    memcpy(fg_app, sp, plen);
                                    fg_app[plen] = '\0';
                                    strncpy(fg_activity, slash+1,
                                            sizeof(fg_activity)-1);
                                    char *end = strpbrk(fg_activity, " }");
                                    if (end) *end = '\0';
                                }
                            }
                        }
                    }
                }
            }
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
    int bat_cap = -1;
    char bat_status[32] = "unknown";

    /* Battery capacity — try common paths across Qualcomm/Xiaomi configurations */
    static const char *cap_paths[] = {
        "/sys/class/power_supply/battery/capacity",
        "/sys/class/power_supply/bms/capacity",
        "/sys/class/power_supply/main/capacity",
        "/sys/class/power_supply/BAT0/capacity",
        NULL
    };
    for (int i = 0; cap_paths[i] && bat_cap < 0; i++) {
        char *s = bexec_read_file(cap_paths[i]);
        if (s) { bat_cap = atoi(s); free(s); }
    }
    /* Final fallback: parse dumpsys battery */
    if (bat_cap < 0) {
        char *bd = bexec("dumpsys battery 2>/dev/null | grep -E '^  level:' | head -1");
        if (bd) {
            sscanf(bd, " level: %d", &bat_cap);
            free(bd);
        }
    }

    /* Battery status — same multi-path approach */
    static const char *stat_paths[] = {
        "/sys/class/power_supply/battery/status",
        "/sys/class/power_supply/bms/status",
        "/sys/class/power_supply/main/status",
        "/sys/class/power_supply/BAT0/status",
        NULL
    };
    for (int i = 0; stat_paths[i] && !strcmp(bat_status,"unknown"); i++) {
        char *s = bexec_read_file(stat_paths[i]);
        if (s) {
            strncpy(bat_status, s, sizeof(bat_status) - 1);
            bat_status[strcspn(bat_status, "\n")] = '\0';
            free(s);
        }
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
    bexec_init();

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
