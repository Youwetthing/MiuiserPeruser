/*
 * bebopd.c — Wakelock & Power Anomaly Monitor
 *
 * Domain: wakelocks and power drain on HyperOS
 *   - Active wakelock list from `dumpsys power`
 *   - Full wakelock detection: non-system holder, duration
 *   - Doze interruption: repeated partial/full wakelock break
 *   - Battery drain estimation
 *   - InCall wakelock orphan: wakelock held without active call
 *
 * Signals emitted:
 *   WAKELOCK_ANOMALY        — unexpected wakelock pattern
 *   WAKELOCK_FULL_HELD      — FULL wakelock held by non-system package
 *   WAKELOCK_DRAIN_HIGH     — excessive wakelock-attributed drain
 *   DOZE_INTERRUPTED        — doze broken repeatedly
 *   INCALL_WAKELOCK_ORPHAN  — InCall wakelock with no active call
 *   BATTERY_LEVEL_CRITICAL  — battery < 10%
 *   BATTERY_SAVER_OFF_LOW   — battery low but saver not active
 *
 * IPC (turtlecom worker):
 *   CAPABILITY?          → CAPABILITY POWER WAKELOCKS
 *                           CAPABILITY POWER DOZE
 *                           CAPABILITY POWER BATTERY
 *   POWER WAKELOCKS      → list of active wake locks
 *   POWER DOZE           → doze state + interruption count
 *   POWER BATTERY        → battery level + saver state
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

#define DAEMON_NAME "bebopd"
#define BUS_PATH    "/data/data/com.termux/files/home/MiuiserPeruser/pipes/turtlecom.sock"

/* Thresholds */
#define BATT_CRITICAL_PCT   10
#define BATT_LOW_PCT        20
#define MAX_WAKELOCK_LINES  64

/* Probe every ~20 seconds */
#define PROBE_TICKS 200

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

/* ── Wakelock data ────────────────────────────────────────────────────── */

typedef struct {
    char  tag[128];
    char  pkg[128];
    int   is_full;     /* FULL vs PARTIAL */
    long  held_ms;
} wakelock_t;

typedef struct {
    wakelock_t  locks[MAX_WAKELOCK_LINES];
    int         count;
    int         full_count;
    int         has_incall;
    char        doze_state[32];
    int         doze_interrupted;   /* incremented if "awake" repeated */
    int         battery_pct;
    int         battery_saver;
} power_state_t;

/* ── System packages whitelist ────────────────────────────────────────── */

static int is_system_pkg(const char *pkg)
{
    return (strstr(pkg, "android") ||
            strstr(pkg, "com.google") ||
            strstr(pkg, "com.miui")   ||
            strstr(pkg, "com.xiaomi") ||
            strstr(pkg, "com.qualcomm") ||
            strstr(pkg, "*media*")    ||
            strstr(pkg, "AudioMix")   ||
            strstr(pkg, "NFC")        ||
            strstr(pkg, "AlarmManager"));
}

/* ── Parse dumpsys power ──────────────────────────────────────────────── */
/*
 * We run dumpsys power via rish (requires Shizuku/ADB).
 * Falls back to unprivileged dumpsys if rish unavailable.
 * Key sections parsed:
 *   - "Wake Locks: size=N"
 *   - "  FULL_WAKE_LOCK 'tag' (package, unimportant=...)"
 *   - "  PARTIAL_WAKE_LOCK 'tag' (package)"
 *   - "mWakefulness=..." for doze state
 *   - battery level from getprop
 */
static power_state_t parse_power_state(void)
{
    power_state_t ps;
    memset(&ps, 0, sizeof(ps));
    strncpy(ps.doze_state, "unknown", sizeof(ps.doze_state));
    ps.battery_pct = -1;

    /* Try rish first, fall back to direct dumpsys */
    const char *cmds[] = {
        "/data/data/com.termux/files/home/.shizuku/rish -c 'dumpsys power 2>/dev/null'",
        "dumpsys power 2>/dev/null",
        NULL
    };

    FILE *f = NULL;
    for (int i = 0; cmds[i] && !f; i++) {
        f = popen(cmds[i], "r");
    }
    if (!f) return ps;

    char line[512];
    int  in_wakelock_section = 0;

    while (fgets(line, sizeof(line), f) && ps.count < MAX_WAKELOCK_LINES) {
        line[strcspn(line, "\n\r")] = '\0';

        /* Wakefulness / doze state */
        char *wk = strstr(line, "mWakefulness=");
        if (wk) {
            sscanf(wk + 13, "%31s", ps.doze_state);
            if (strcmp(ps.doze_state, "Awake") == 0)
                ps.doze_interrupted++;
            continue;
        }

        /* Battery saver */
        if (strstr(line, "mIsPowered=true") || strstr(line, "mBatterySaverEnabled=true"))
            ps.battery_saver = 1;

        /* Enter/exit wake lock section */
        if (strstr(line, "Wake Locks:")) { in_wakelock_section = 1; continue; }
        if (in_wakelock_section && line[0] != ' ' && line[0] != '\t')
            in_wakelock_section = 0;

        if (!in_wakelock_section) continue;

        /* Parse wakelock line:
         *   "  PARTIAL_WAKE_LOCK 'tag' (pkg, unimportant=false, workSource=...)"
         *   or
         *   "  FULL_WAKE_LOCK    'tag' (pkg)"
         */
        int is_full = (strstr(line, "FULL_WAKE_LOCK") != NULL);
        int is_part = (strstr(line, "PARTIAL_WAKE_LOCK") != NULL);
        if (!is_full && !is_part) continue;

        wakelock_t *wl = &ps.locks[ps.count];
        wl->is_full = is_full;

        /* Extract tag between single quotes */
        char *q1 = strchr(line, '\'');
        if (q1) {
            char *q2 = strchr(q1 + 1, '\'');
            if (q2) {
                int len = (int)(q2 - q1 - 1);
                if (len >= (int)sizeof(wl->tag)) len = sizeof(wl->tag) - 1;
                strncpy(wl->tag, q1 + 1, (size_t)len);
                wl->tag[len] = '\0';
            }
        }

        /* Extract package between first '(' and ',' or ')' */
        char *p1 = strchr(line, '(');
        if (p1) {
            char *p2 = strpbrk(p1 + 1, ",)");
            if (p2) {
                int len = (int)(p2 - p1 - 1);
                if (len >= (int)sizeof(wl->pkg)) len = sizeof(wl->pkg) - 1;
                strncpy(wl->pkg, p1 + 1, (size_t)len);
                wl->pkg[len] = '\0';
                /* Trim whitespace */
                while (wl->pkg[0] == ' ') memmove(wl->pkg, wl->pkg + 1, strlen(wl->pkg));
            }
        }

        if (is_full) ps.full_count++;
        if (strstr(wl->tag, "InCall") || strstr(wl->pkg, "telecom") ||
            strstr(wl->pkg, "phone"))
            ps.has_incall = 1;

        ps.count++;
    }
    pclose(f);

    /* Battery level via getprop */
    FILE *bf = popen("getprop 'sys.battery.level' 2>/dev/null || "
                     "cat /sys/class/power_supply/battery/capacity 2>/dev/null", "r");
    if (bf) {
        fscanf(bf, "%d", &ps.battery_pct);
        pclose(bf);
    }

    /* Battery saver via getprop if not found in dumpsys */
    if (!ps.battery_saver) {
        FILE *sf = popen("getprop 'persist.sys.battery.saver' 2>/dev/null", "r");
        if (sf) {
            char sbuf[8] = {0};
            fgets(sbuf, sizeof(sbuf), sf);
            ps.battery_saver = (sbuf[0] == '1');
            pclose(sf);
        }
    }

    return ps;
}

/* ── Check for active phone call ──────────────────────────────────────── */

static int is_call_active(void)
{
    FILE *f = popen("getprop 'gsm.call.state' 2>/dev/null", "r");
    if (!f) return 0;
    char buf[16] = {0};
    fgets(buf, sizeof(buf), f);
    pclose(f);
    /* 0 = idle, 1 = ringing, 2 = active */
    return (buf[0] != '0' && buf[0] != '\0' && buf[0] != '\n');
}

/* ── Format wakelock list for IPC ─────────────────────────────────────── */

static void format_wakelock_list(const power_state_t *ps, char *out, size_t len)
{
    out[0] = '\0';
    for (int i = 0; i < ps->count && strlen(out) + 80 < len; i++) {
        char item[96];
        snprintf(item, sizeof(item), "[%s]%s(%s) ",
                 ps->locks[i].is_full ? "FULL" : "PART",
                 ps->locks[i].tag,
                 ps->locks[i].pkg);
        strncat(out, item, len - strlen(out) - 1);
    }
    if (!out[0]) strncpy(out, "(none)", len);
}

/* ── Probe & emit ─────────────────────────────────────────────────────── */

static int g_prev_doze_interrupted = 0;

static void probe_and_emit(int ipc_fd, const char *cmd)
{
    power_state_t ps = parse_power_state();

    char lock_list[1024] = {0};
    format_wakelock_list(&ps, lock_list, sizeof(lock_list));

    daemon_log_info("Power: locks=%d full=%d doze=%s batt=%d%% saver=%d",
                    ps.count, ps.full_count,
                    ps.doze_state, ps.battery_pct, ps.battery_saver);

    /* ── IPC responses ─────────────────────────────────────────────── */
    if (ipc_fd >= 0 && cmd) {
        if (strncmp(cmd, "POWER WAKELOCKS", 15) == 0) {
            dprintf(ipc_fd, "POWER WAKELOCKS count=%d full=%d %s\n",
                    ps.count, ps.full_count, lock_list);
        } else if (strncmp(cmd, "POWER DOZE", 10) == 0) {
            dprintf(ipc_fd, "POWER DOZE state=%s interrupted=%d\n",
                    ps.doze_state, ps.doze_interrupted);
        } else if (strncmp(cmd, "POWER BATTERY", 13) == 0) {
            dprintf(ipc_fd, "POWER BATTERY level=%d%% saver=%d\n",
                    ps.battery_pct, ps.battery_saver);
        }
    }

    /* ── Signal evaluation ─────────────────────────────────────────── */

    /* Full wakelock held by non-system package */
    for (int i = 0; i < ps.count; i++) {
        if (!ps.locks[i].is_full) continue;
        if (!is_system_pkg(ps.locks[i].pkg)) {
            char ctx[256];
            snprintf(ctx, sizeof(ctx), "tag=%s pkg=%s",
                     ps.locks[i].tag, ps.locks[i].pkg);
            gaveld_emit(DAEMON_NAME, "WAKELOCK_FULL_HELD", 1.0f, ctx);
            splinterd_emit("wakelock_full_held", ctx);
            break;   /* one signal per poll */
        }
    }

    /* Anomalous wakelock count */
    if (ps.count > 10) {
        char ctx[64];
        snprintf(ctx, sizeof(ctx), "count=%d full=%d", ps.count, ps.full_count);
        gaveld_emit(DAEMON_NAME, "WAKELOCK_ANOMALY", (float)ps.count, ctx);
        splinterd_emit("wakelock_anomaly", ctx);
    }

    /* Doze interrupted: track delta */
    int doze_delta = ps.doze_interrupted - g_prev_doze_interrupted;
    if (doze_delta > 3) {
        char ctx[64];
        snprintf(ctx, sizeof(ctx), "interruptions_this_poll=%d total=%d",
                 doze_delta, ps.doze_interrupted);
        gaveld_emit(DAEMON_NAME, "DOZE_INTERRUPTED", (float)doze_delta, ctx);
        splinterd_emit("doze_interrupted", ctx);
    }
    g_prev_doze_interrupted = ps.doze_interrupted;

    /* InCall wakelock without active call */
    if (ps.has_incall && !is_call_active()) {
        gaveld_emit(DAEMON_NAME, "INCALL_WAKELOCK_ORPHAN", 1.0f,
                    "InCall wakelock held with no active call");
        splinterd_emit("incall_wakelock_orphan",
                       "InCall wakelock with no active call");
    }

    /* Battery critical */
    if (ps.battery_pct >= 0 && ps.battery_pct < BATT_CRITICAL_PCT) {
        char ctx[32];
        snprintf(ctx, sizeof(ctx), "level=%d%%", ps.battery_pct);
        gaveld_emit(DAEMON_NAME, "BATTERY_LEVEL_CRITICAL", (float)ps.battery_pct, ctx);
        splinterd_emit("battery_critical", ctx);
    } else if (ps.battery_pct >= 0 && ps.battery_pct < BATT_LOW_PCT &&
               !ps.battery_saver) {
        char ctx[32];
        snprintf(ctx, sizeof(ctx), "level=%d%%", ps.battery_pct);
        gaveld_emit(DAEMON_NAME, "BATTERY_SAVER_OFF_LOW", (float)ps.battery_pct, ctx);
        splinterd_emit("battery_saver_off_low", ctx);
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

    write(fd, "HELLO WORKER BEBOP\n", 19);
    daemon_log_info(DAEMON_NAME " ONLINE — Wakelock & Power Monitor");

    char buf[512];
    int  tick = 0;

    for (;;) {
        usleep(100000);   /* 100ms */
        tick++;

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
            write(fd, "CAPABILITY POWER WAKELOCKS\n", 27);
            write(fd, "CAPABILITY POWER DOZE\n",      22);
            write(fd, "CAPABILITY POWER BATTERY\n",   25);
            continue;
        }

        if (strncmp(buf, "POWER", 5) == 0) {
            probe_and_emit(fd, buf);
            continue;
        }
    }

    return 0;
}
