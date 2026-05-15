/*
 * bebopd.c — Wake Lock & Alarm Warden
 *
 * Every poll:
 *   - Parse active wake locks from /sys/power/wake_lock + dumpsys power
 *   - Count partial vs full wake locks, identify holders
 *   - Parse pending alarms, identify top setters
 *   - Emit APRIL anomaly events to splinterd when thresholds exceeded
 *
 * APRIL events emitted:
 *   wakelock   — lock count, longest holder
 *   alarm      — pending alarm count, top setter
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

#define DAEMON_NAME      "bebopd"
#define POLL_SEC         15
#define WAKELOCK_WARN    5    /* alert if >= this many partial wakelocks */
#define ALARM_WARN       25   /* alert if >= this many pending alarms */

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

/* run_cmd: thin alias over the privileged backend */
static char *run_cmd(const char *cmd) { return bexec(cmd); }

static int count_lines_containing(const char *haystack, const char *needle)
{
    if (!haystack || !needle) return 0;
    int count = 0;
    const char *p = haystack;
    while ((p = strstr(p, needle)) != NULL) {
        count++;
        p++;
    }
    return count;
}

/* Extract first match of needle, up to max chars after it */
static int extract_after(const char *hay, const char *needle,
                          char *out, size_t outlen)
{
    const char *p = strstr(hay, needle);
    if (!p) return 0;
    p += strlen(needle);
    while (*p == ' ' || *p == '=' || *p == ':') p++;
    size_t i = 0;
    while (*p && *p != '\n' && *p != '\r' && i < outlen - 1)
        out[i++] = *p++;
    out[i] = '\0';
    return i > 0;
}

/* ── Wake lock audit ──────────────────────────────────────────────────── */

static void poll_wakelocks(void)
{
    /* /sys/power/wake_lock lists currently held locks (space-separated) */
    FILE *f = fopen("/sys/power/wake_lock", "r");
    int sysfs_ok = 0, sysfs_count = 0;
    char wl_names[256] = "(none)";

    if (f) {
        char line[512];
        if (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\n")] = '\0';
            if (strlen(line) > 0) {
                strncpy(wl_names, line, sizeof(wl_names) - 1);
                /* Count space-separated tokens */
                char *tok = strtok(line, " \t");
                while (tok) { sysfs_count++; tok = strtok(NULL, " \t"); }
                sysfs_ok = 1;
            }
        }
        fclose(f);
    }

    /* dumpsys power for named partial wake lock detail */
    char *dump = run_cmd("dumpsys power 2>/dev/null");
    int partial = 0, full = 0;
    char top_holder[128] = "unknown";

    if (dump) {
        partial  = count_lines_containing(dump, "PARTIAL_WAKE_LOCK");
        full     = count_lines_containing(dump, "FULL_WAKE_LOCK");

        /* Wake lock holder — try several field names used across MIUI versions:
         *   tag="com.package" / PackageName=com.package / pkg=com.package
         *   ACQ_TIME / ACQUIRE_CAUSE lines contain the holder package         */
        const char *holder_keys[] = { "tag=\"", "PackageName=", "pkg=",
                                       "WorkSource{", NULL };
        for (int ki = 0; holder_keys[ki] && top_holder[0] == '\0'; ki++) {
            const char *h = strstr(dump, holder_keys[ki]);
            if (!h) continue;
            h += strlen(holder_keys[ki]);
            /* strip leading quote if tag="..." */
            if (*h == '"') h++;
            size_t i = 0;
            while (*h && *h != '"' && *h != ' ' && *h != '\n'
                       && *h != '}' && i < 127)
                top_holder[i++] = *h++;
            top_holder[i] = '\0';
        }

        /* Also try parsing individual PARTIAL_WAKE_LOCK lines for a name */
        if (!top_holder[0]) {
            const char *pw = strstr(dump, "PARTIAL_WAKE_LOCK");
            if (pw) {
                /* Line typically: "PARTIAL_WAKE_LOCK 'tag' pid=X ws=..." */
                const char *sq = strchr(pw, '\'');
                if (sq) {
                    sq++;
                    size_t i = 0;
                    while (*sq && *sq != '\'' && i < 127)
                        top_holder[i++] = *sq++;
                    top_holder[i] = '\0';
                }
            }
        }

        free(dump);
    }

    int total = sysfs_ok ? sysfs_count : (partial + full);

    printf("[BEBOP] Wake Locks: %d active  (partial=%d full=%d)\n",
           total, partial, full);
    printf("[BEBOP] Top holder : %s\n", top_holder);
    if (sysfs_ok)
        printf("[BEBOP] Sysfs lock names: %.120s\n", wl_names);

    if (total >= WAKELOCK_WARN) {
        printf("[BEBOP] *** ANOMALY: %d wake locks held — system may be prevented from sleeping\n",
               total);
        char ev[256];
        snprintf(ev, sizeof(ev),
                 "count=%d partial=%d full=%d holder=%.64s",
                 total, partial, full, top_holder);
        splinterd_emit("wakelock", ev);
    }
}

/* ── Alarm audit ──────────────────────────────────────────────────────── */

static void poll_alarms(void)
{
    char *dump = run_cmd("dumpsys alarm 2>/dev/null");
    if (!dump) {
        printf("[BEBOP] Alarms  : dump unavailable\n");
        return;
    }

    /* Count "ELAPSED_REALTIME_WAKEUP" and "RTC_WAKEUP" alarm lines */
    int wakeup = count_lines_containing(dump, "ELAPSED_REALTIME_WAKEUP")
               + count_lines_containing(dump, "RTC_WAKEUP");
    int total  = count_lines_containing(dump, "type=");

    /* Identify top alarm setter — try multiple field names MIUI uses */
    char top_pkg[128] = "unknown";
    {
        /* Field names across Android/MIUI versions:
         *   packageName=com.pkg  /  pkg=com.pkg  /  package com.pkg
         *   uid=u0a123 (needs UID map)  */
        static const char *pkg_keys[] = {
            "packageName=", "pkg=", "package=", NULL
        };
        int best = 0;
        for (int ki = 0; pkg_keys[ki]; ki++) {
            const char *p = dump;
            char pkg[128];
            while ((p = strstr(p, pkg_keys[ki])) != NULL) {
                p += strlen(pkg_keys[ki]);
                /* skip quotes */
                if (*p == '"' || *p == '\'') p++;
                size_t i = 0;
                while (p[i] && p[i] != ' ' && p[i] != '\n' &&
                       p[i] != '"' && p[i] != '\'' && i < 127)
                    pkg[i] = p[i++];
                pkg[i] = '\0';
                if (!i || !strchr(pkg, '.')) continue; /* skip non-package tokens */
                char search[160];
                snprintf(search, sizeof(search), "%s%s", pkg_keys[ki], pkg);
                int cnt = count_lines_containing(dump, search);
                if (cnt > best) {
                    best = cnt;
                    strncpy(top_pkg, pkg, sizeof(top_pkg) - 1);
                }
            }
        }
    }

    /* Next alarm trigger time */
    char next_time[64] = "N/A";
    extract_after(dump, "nextWakeup", next_time, sizeof(next_time));
    if (!next_time[0])
        extract_after(dump, "Next", next_time, sizeof(next_time));

    printf("[BEBOP] Alarms  : %d total, %d are wakeup alarms\n",
           total, wakeup);
    printf("[BEBOP] Top pkg : %s\n", top_pkg);
    printf("[BEBOP] Next wakeup: %s\n", next_time);

    if (wakeup >= ALARM_WARN) {
        printf("[BEBOP] *** ANOMALY: %d pending wakeup alarms\n", wakeup);
        char ev[256];
        snprintf(ev, sizeof(ev),
                 "wakeup_alarms=%d total=%d top_pkg=%.64s",
                 wakeup, total, top_pkg);
        splinterd_emit("alarm", ev);
    }

    free(dump);
}

/* ── Main ─────────────────────────────────────────────────────────────── */

int main(void)
{
    if (!daemon_core_init(DAEMON_NAME)) return 1;
    bexec_init();

    char ts[32];
    time_t t;
    int cycle = 0;

    for (;;) {
        t = time(NULL);
        strftime(ts, sizeof(ts), "%H:%M:%S", localtime(&t));

        printf("\n[BEBOP] ── Poll #%d  %s ──────────────────────────\n",
               ++cycle, ts);

        poll_wakelocks();
        poll_alarms();

        printf("[BEBOP] Next poll in %ds\n", POLL_SEC);
        sleep(POLL_SEC);
    }

    daemon_core_shutdown();
    return 0;
}
