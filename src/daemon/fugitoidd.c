/*
 * fugitoidd.c — Foreground Activity Bridge & System Event Monitor
 *
 * Every poll (single consolidated rish call):
 *   - Detect foreground app from dumpsys window/activity
 *   - Scan logcat tail for crashes, ANRs, OOM kills
 *   - Track app switches
 *   - Read system memory headline from /proc
 *   - Emit gaveld signals on ANR, crash, OOM
 *
 * Gaveld signals:
 *   ANR_DETECTED, CRASH_DETECTED, OOM_KILL_EVENT, APP_SWITCH_ANOMALY
 *
 * Runtime config: enabled, interval (default 20), scan_count
 */

#include "ipc_globals.h"
#include "gaveld_emit.h"
#include "backend_exec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdbool.h>
#include <errno.h>
#include <stdarg.h>

#define DAEMON_NAME   "fugitoidd"
#define DEFAULT_INTERVAL 20
#define LOGCAT_LINES  30

#ifndef MP_BASE_DIR
#define MP_BASE_DIR "/data/data/com.termux/files/home/MiuiserPeruser"
#endif

#define STATE_FILE   MP_BASE_DIR "/Registry/daemon_state.json"
#define RESULTS_DIR  MP_BASE_DIR "/Registry/daemon_results"
#define RESULTS_FILE RESULTS_DIR "/" DAEMON_NAME ".json"

/* ── Logging (burned/shredderd-style: stderr always, optional file dest) ── */

static FILE *g_fugitoid_log_fp = NULL;

static void fugitoidlog_init(void)
{
    const char *path = getenv("FUGITOID_LOG_PATH");
    if (path && *path) {
        g_fugitoid_log_fp = fopen(path, "a");
        if (!g_fugitoid_log_fp) {
            fprintf(stderr, "[FUGITOID] WARN: cannot open log file %s: %s\n",
                    path, strerror(errno));
        }
    }
}

/* NOTE: uses a separate va_start/va_end per output destination.
 * Reusing one va_list across two vfprintf() calls is undefined
 * behavior — this bit krangd once already; don't repeat it here. */
static void fugitoidlog(const char *level, const char *fmt, ...)
{
    time_t t = time(NULL);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", localtime(&t));

    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[%s][FUGITOID/%s] ", ts, level);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);

    if (g_fugitoid_log_fp) {
        va_list ap2;
        va_start(ap2, fmt);
        fprintf(g_fugitoid_log_fp, "[%s][FUGITOID/%s] ", ts, level);
        vfprintf(g_fugitoid_log_fp, fmt, ap2);
        fprintf(g_fugitoid_log_fp, "\n");
        va_end(ap2);
        fflush(g_fugitoid_log_fp);
    }
}

static char g_prev_app[128] = {0};

/* ── Config ───────────────────────────────────────────────────────────── */

static int config_get_int(const char *key, int def)
{
    FILE *f = fopen(STATE_FILE, "r");
    if (!f) return def;
    char buf[4096] = {0};
    fread(buf, 1, sizeof(buf)-1, f);
    fclose(f);
    /* Find "DAEMON_NAME": { ... "key": VALUE */
    char section[64];
    snprintf(section, sizeof(section), "\"%s\"", DAEMON_NAME);
    char *s = strstr(buf, section);
    if (!s) return def;
    char field[64];
    snprintf(field, sizeof(field), "\"%s\":", key);
    char *p = strstr(s, field);
    if (!p) return def;
    p += strlen(field);
    while (*p == ' ' || *p == '\t') p++;
    return atoi(p);
}

static int is_enabled(void)    { return config_get_int("enabled",    1); }
static int get_interval(void)  { return config_get_int("interval",   DEFAULT_INTERVAL); }
static int get_max_scans(void) { return config_get_int("scan_count", 0); }

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

static int count_substr(const char *hay, const char *needle)
{
    if (!hay || !needle) return 0;
    int c = 0;
    const char *p = hay;
    while ((p = strstr(p, needle)) != NULL) { c++; p++; }
    return c;
}

/* Neutralize chars that would break JSON string values or the APRIL
 * pipe-delimited protocol if they ever slip through from raw shell
 * output (same bug class fixed in granitord.c). */
static void sanitize_field(char *s)
{
    if (!s) return;
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c < 0x20 || c == '"' || c == '|' || c == '\\')
            *s = '_';
    }
}

/* ── Results writer ───────────────────────────────────────────────────── */

static void write_results(int scan_num, int crashes, int anrs,
                           int ooms, const char *fg_app)
{
    FILE *f = fopen(RESULTS_FILE, "w");
    if (!f) return;
    time_t t = time(NULL);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", localtime(&t));
    fprintf(f,
        "{\n"
        "  \"daemon\": \"" DAEMON_NAME "\",\n"
        "  \"timestamp\": \"%s\",\n"
        "  \"scan_number\": %d,\n"
        "  \"foreground_app\": \"%.64s\",\n"
        "  \"crashes\": %d,\n"
        "  \"anrs\": %d,\n"
        "  \"oom_events\": %d\n"
        "}\n",
        ts, scan_num, fg_app ? fg_app : "unknown",
        crashes, anrs, ooms);
    fclose(f);
}

/* ── Main poll ────────────────────────────────────────────────────────── */

static void poll(int scan_num)
{
    char ts[32];
    time_t t = time(NULL);
    strftime(ts, sizeof(ts), "%H:%M:%S", localtime(&t));

    fugitoidlog("INFO", "── System Bridge #%d  %s ─────────────────────", scan_num, ts);

    /* ── Single rish call: foreground + main/crash/ANR logcat (4-way split) ──
     * Crash/ANR now pulled from dedicated buffers instead of the noisy main
     * *:W tail -- a crash entry visible on one poll was previously observed
     * to fall out of a fresh *:W pull one poll later, displaced by unrelated
     * warning spam, causing crash-target extraction to return "unknown"
     * despite crashes being correctly counted. */
    char combined[2048];
    snprintf(combined, sizeof(combined),
             "echo ==FG==;"
             "dumpsys activity activities 2>/dev/null"
             " | grep -E 'topResumedActivity|mResumedActivity' | head -2;"
             "echo ==MAIN==;"
             "logcat -d -t %d *:W 2>/dev/null;"
             "echo ==CRASH==;"
             "logcat -d -b crash -t %d 2>/dev/null;"
             "echo ==ANR==;"
             "logcat -d -b system -t %d 2>/dev/null"
             " | grep -iE 'ANR in|ANR Warning'",
             LOGCAT_LINES, LOGCAT_LINES, LOGCAT_LINES);

    char *raw = bexec(combined);

    /* ── Foreground app ───────────────────────────────────────────────── */
    char fg_app[128] = "unknown";

    if (raw) {
        char *fg_sec    = strstr(raw, "==FG==");
        char *main_sec  = strstr(raw, "==MAIN==");
        char *crash_sec = strstr(raw, "==CRASH==");
        char *anr_sec   = strstr(raw, "==ANR==");

        if (fg_sec) {
            fg_sec += 6;
            /* Null-terminate fg section */
            if (main_sec) {
                char *e = strstr(fg_sec, "==MAIN==");
                if (e) *e = '\0';
            }

            /* Extract package from topResumedActivity=ActivityRecord{... u0 pkg/act} */
            const char *p = strstr(fg_sec, "ActivityRecord{");
            if (p) {
                const char *u0 = strstr(p, " u0 ");
                if (u0) {
                    u0 += 4;
                    size_t i = 0;
                    char tmp[128] = {0};
                    while (*u0 && *u0 != ' ' && *u0 != '}' && i < 127)
                        tmp[i++] = *u0++;
                    tmp[i] = '\0';
                    char *slash = strchr(tmp, '/');
                    if (slash) *slash = '\0';
                    if (tmp[0]) strncpy(fg_app, tmp, sizeof(fg_app) - 1);
                    sanitize_field(fg_app);
                }
            }
        }

        /* Advance section pointers past their tags, and null-terminate
         * each section at the start of the next tag so parsing doesn't
         * bleed across buffers (same anchored-before-nulling pattern used
         * in shredderd's probe_section splitting). */
        if (main_sec) {
            main_sec += 8;
            if (crash_sec) {
                char *e = strstr(main_sec, "==CRASH==");
                if (e) *e = '\0';
            }
        }
        if (crash_sec) {
            crash_sec += 9;
            if (anr_sec) {
                char *e = strstr(crash_sec, "==ANR==");
                if (e) *e = '\0';
            }
        }
        if (anr_sec) anr_sec += 7;

        fugitoidlog("INFO", "Foreground : %s", fg_app);

        /* App switch detection */
        if (strcmp(fg_app, "unknown") != 0 &&
            strcmp(fg_app, g_prev_app) != 0 && g_prev_app[0]) {
            fugitoidlog("INFO", "App switch : %s -> %s", g_prev_app, fg_app);
            char ev[256];
            snprintf(ev, sizeof(ev), "from=%.48s to=%.48s", g_prev_app, fg_app);
            gaveld_emit(DAEMON_NAME, "APP_SWITCH_ANOMALY", 0.0, ev);
            splinterd_emit("app_switch", ev);
        }
        if (strcmp(fg_app, "unknown") != 0)
            strncpy(g_prev_app, fg_app, sizeof(g_prev_app) - 1);

        /* ── Logcat analysis (crash/ANR from dedicated buffers, OOM/watchdog
         * stay on the main *:W tail) ────────────────────────────────────── */
        const char *main_log  = main_sec  ? main_sec  : "";
        const char *crash_log = crash_sec ? crash_sec : "";
        const char *anr_log   = anr_sec   ? anr_sec   : "";

        int crashes = count_substr(crash_log, "FATAL EXCEPTION")
                    + count_substr(main_log, "crashed service");
        int anrs    = count_substr(anr_log, "ANR in")
                    + count_substr(anr_log, "ANR Warning");
        int ooms    = count_substr(main_log, "lowmemorykiller")
                    + count_substr(main_log, "OOM killer")
                    + count_substr(main_log, "mem-pressure-event");
        int wdogs   = count_substr(main_log, "watchdog");

        fugitoidlog("INFO", "Logcat     : crashes=%-3d  ANR=%-3d  OOM=%-3d  watchdog=%d",
                    crashes, anrs, ooms, wdogs);

        if (anrs > 0) {
            char target[64] = "unknown";
            const char *p2 = strstr(anr_log, "ANR in ");
            if (p2) {
                p2 += 7;
                size_t i = 0;
                while (*p2 && *p2 != '\n' && *p2 != ' ' && i < 63)
                    target[i++] = *p2++;
                target[i] = '\0';
            }
            sanitize_field(target);
            char ev[256];
            snprintf(ev, sizeof(ev), "count=%d target=%.48s", anrs, target);
            gaveld_emit(DAEMON_NAME, "ANR_DETECTED", 0.0, ev);
            splinterd_emit("anr_detected", ev);
            fugitoidlog("WARN", "ANR: %s", target);
        }

        if (crashes > 0) {
            char target[64] = "unknown";
            const char *p2 = strstr(crash_log, "FATAL EXCEPTION:");
            if (p2) {
                p2 = strstr(p2, "Process: ");
                if (p2) {
                    p2 += 9;
                    size_t i = 0;
                    while (*p2 && *p2 != '\n' && *p2 != ',' && i < 63)
                        target[i++] = *p2++;
                    target[i] = '\0';
                }
            }
            sanitize_field(target);
            char ev[256];
            snprintf(ev, sizeof(ev), "count=%d process=%.48s", crashes, target);
            gaveld_emit(DAEMON_NAME, "CRASH_DETECTED", 0.0, ev);
            splinterd_emit("crash_detected", ev);
            fugitoidlog("WARN", "CRASH: %s", target);
        }

        if (ooms > 0) {
            char ev[64];
            snprintf(ev, sizeof(ev), "oom_events=%d", ooms);
            gaveld_emit(DAEMON_NAME, "OOM_KILL_EVENT", 0.0, ev);
            splinterd_emit("oom_kill", ev);
            fugitoidlog("WARN", "OOM kill: %d event(s)", ooms);
        }

        write_results(scan_num, crashes, anrs, ooms, fg_app);
        free(raw);
    } else {
        fugitoidlog("WARN", "rish unavailable — limited mode");
    }

    /* ── Memory headline from /proc (no rish needed) ──────────────────── */
    long avail_mb = 0;
    FILE *mf = fopen("/proc/meminfo", "r");
    if (mf) {
        char line[128]; long v;
        while (fgets(line, sizeof(line), mf))
            if (sscanf(line, "MemAvailable: %ld", &v) == 1)
                { avail_mb = v / 1024; break; }
        fclose(mf);
    }
    fugitoidlog("INFO", "MemFree    : %ldMB available", avail_mb);
}

/* ── Main ─────────────────────────────────────────────────────────────── */

int main(void)
{
    bexec_init();
    fugitoidlog_init();

    if (!is_enabled()) {
        fugitoidlog("INFO", "disabled via syndicatectl — exiting");
        return 0;
    }

    fugitoidlog("INFO", "Foreground Activity & System Event Monitor: ONLINE");
    int interval  = get_interval();
    int max_scans = get_max_scans();
    int scan_num  = 0;

    for (;;) {
        if (!is_enabled()) {
            fugitoidlog("INFO", "disabled — stopping");
            break;
        }

        interval  = get_interval();
        max_scans = get_max_scans();
        scan_num++;

            poll(scan_num);
    
        if (max_scans > 0 && scan_num >= max_scans) {
            fugitoidlog("INFO", "reached scan_count=%d — exiting", max_scans);
            break;
        }

        fugitoidlog("INFO", "Next poll in %ds", interval);
        sleep(interval);
    }

    return 0;
}
