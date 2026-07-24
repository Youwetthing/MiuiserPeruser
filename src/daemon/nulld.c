/*
 * nulld.c — Idle Transmission Monitor
 *
 * The invisible corporate surveillance daemon.
 * Watches what Xiaomi does while you sleep.
 *
 * Method: poll screen state every 10s. When screen goes off,
 * baseline TCP connections + netstats per-UID. Watch for spikes.
 * Correlate with which UIDs are transmitting during idle.
 *
 * Named after the Null villain — invisible, corporate, always watching.
 *
 * v1.1 — 2026-07-11 fixes:
 *   - Routed through the shared bexec() abstraction instead of a
 *     hardcoded adb_cli-only popen (survives dropped ADB pairing via
 *     rish failover, same as every other daemon in the fleet)
 *   - Added sensor_health JSON field (adb_reachable, consecutive_failures)
 *     so a dead privileged backend produces an explicit signal instead of
 *     silent zeros indistinguishable from legitimate idle data
 *   - Fixed: main() never set g_running = true, so the poll loop never
 *     ran a single time regardless of backend state — found and fixed
 *     2026-07-11 after this exact daemon repeatedly showed "no data" all
 *     night for what turned out to be an unrelated reason
 */

#include "daemon_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <stdint.h>

#include "ipc_globals.h"
#include "gaveld_emit.h"
#include "backend_exec.h"

#define BASE         "/data/data/com.termux/files/home/MiuiserPeruser"
#define RESULTS_FILE  BASE "/Registry/daemon_results/nulld.json"
#define BASELINE_FILE BASE "/data/nulld_baseline.json"
#define PID_FILE      BASE "/pipes/pids/nulld.pid"
#define POLL_SEC      10
#define IDLE_SPIKE_THRESHOLD  5   /* TCP connection increase while idle */
#define IDLE_BYTES_THRESHOLD  102400 /* 100KB transmitted while idle */

typedef enum { SCREEN_UNKNOWN, SCREEN_ON, SCREEN_OFF } screen_state_t;

typedef struct {
    int    tcp4;
    int    tcp6;
    long   rx_bytes;
    long   tx_bytes;
    time_t ts;
} net_snapshot_t;

static screen_state_t g_screen      = SCREEN_UNKNOWN;
static screen_state_t g_prev_screen = SCREEN_UNKNOWN;
static net_snapshot_t g_idle_baseline = {0};
static int            g_idle_secs   = 0;
static int            g_spike_count = 0;
static int            g_total_events = 0;

/* ADB/adb_cli reachability tracking — without this, a dropped ADB
 * connection (e.g. wireless debugging re-pairing, port change) produces
 * silent zeros indistinguishable from legitimate "all quiet" idle data.
 * check_adb_health() round-trips a known string through the same
 * null_run() path every real command uses, so it reflects true reachability. */
static int g_adb_healthy         = 1;
static int g_consecutive_failures = 0;
static time_t g_last_health_check = 0;

/* Known Xiaomi/MIUI system UIDs that shouldn't transmit while idle */
static const char *SUSPICIOUS_IDLE_PKGS[] = {
    "com.miui.analytics",
    "com.xiaomi.market",
    "com.miui.daemon",
    "com.xiaomi.finddevice",
    "com.miui.systemAdSolution",
    "com.miui.cloudbackup",
    "com.miui.cloudservice",
    "com.miui.stats",
    NULL
};

static void tlog(const char *lvl, const char *msg) {
    char ts[32]; time_t t = time(NULL);
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", localtime(&t));
    fprintf(stderr, "[%s][NULLD/%s] %s\n", ts, lvl, msg);
    fflush(stderr);
}

static char *null_run(const char *cmd) {
    /* Routed through the shared bexec() abstraction instead of a hardcoded
     * adb_cli-only popen. bexec() already probes rish -> adb_cli -> adb ->
     * direct with a disk-cached backend choice (backend_exec.c), so nulld
     * now survives a dropped ADB pairing the same way every other daemon
     * in the fleet does, instead of going fully dark the instant ADB drops
     * even while Shizuku/rish is alive and reachable. */
    char *out = bexec(cmd);
    if (!out) return NULL;
    if (strlen(out) == 0) { free(out); return NULL; }
    return out;
}

/* ── Backend health check ─────────────────────────────────────── *
 * Reflects whether ANY bexec backend (rish, adb_cli, adb) is currently
 * reachable — not ADB specifically. bexec() may resolve via rish even
 * when ADB is fully down, so this check (and the resulting sensor_health
 * JSON field) intentionally covers the whole privileged-access path, not
 * just one transport. */
static int check_adb_health(void) {
    char *out = null_run("echo adb_ok");
    int healthy = (out && strstr(out, "adb_ok") != NULL);
    if (out) free(out);

    if (healthy) {
        if (!g_adb_healthy) {
            char msg[128];
            snprintf(msg, sizeof(msg),
                "Privileged backend restored after %d failed check(s)",
                g_consecutive_failures);
            tlog("INFO", msg);
        }
        g_consecutive_failures = 0;
    } else {
        g_consecutive_failures++;
        char msg[128];
        snprintf(msg, sizeof(msg),
            "No privileged backend reachable — consecutive_failures=%d "
            "(all readings this cycle are stale/zero, not real idle data)",
            g_consecutive_failures);
        tlog("WARN", msg);
    }

    g_adb_healthy = healthy;
    g_last_health_check = time(NULL);
    return healthy;
}

/* ── Screen state ─────────────────────────────────────────────── */
static screen_state_t get_screen_state(void) {
    char *out = null_run("dumpsys power | grep mWakefulness");
    if (!out) return SCREEN_UNKNOWN;
    screen_state_t s = SCREEN_UNKNOWN;
    if (strstr(out, "Awake"))  s = SCREEN_ON;
    if (strstr(out, "Asleep") || strstr(out, "Dozing")) s = SCREEN_OFF;
    free(out);
    return s;
}

/* ── TCP connection count ─────────────────────────────────────── */
static void get_tcp_counts(int *tcp4, int *tcp6) {
    *tcp4 = *tcp6 = 0;
    char *t4 = null_run("cat /proc/net/tcp 2>/dev/null | wc -l");
    char *t6 = null_run("cat /proc/net/tcp6 2>/dev/null | wc -l");
    if (t4) { *tcp4 = atoi(t4) - 1; free(t4); }
    if (t6) { *tcp6 = atoi(t6) - 1; free(t6); }
}

/* ── Network bytes via netstats ───────────────────────────────── */
static void get_net_bytes(long *rx, long *tx) {
    *rx = *tx = 0;
    char *out = null_run(
        "cat /proc/net/dev 2>/dev/null | grep wlan0");
    if (!out) return;
    /* format: iface: rx_bytes rx_pkts ... tx_bytes tx_pkts */
    char *p = strchr(out, ':');
    if (p) {
        long rb=0, rp=0, re=0, rd=0, rr=0, rf=0, rg=0, rh=0, tb=0;
        sscanf(p+1, "%ld %ld %ld %ld %ld %ld %ld %ld %ld",
               &rb, &rp, &re, &rd, &rr, &rf, &rg, &rh, &tb);
        *rx = rb; *tx = tb;
    }
    free(out);
}

/* ── Per-package session snapshot ────────────────────────────── */
typedef struct { char pkg[128]; int uid; int sessions; } pkg_sessions_t;
static pkg_sessions_t g_baseline_sessions[64];
static int            g_baseline_session_count = 0;

static int get_package_sessions(pkg_sessions_t *out, int max) {
    char *raw = null_run(
        "dumpsys netstats | grep \"uid=.*package=\"");
    if (!raw) return 0;
    int count = 0;
    char *line = strtok(raw, "\n");
    while (line && count < max) {
        int uid = 0; char pkg[128] = {0}; int sessions = 0;
        if (sscanf(line, " {uid=%d,package=%127[^}]}=%d", &uid, pkg, &sessions) == 3) {
            out[count].uid = uid;
            strncpy(out[count].pkg, pkg, 127);
            out[count].sessions = sessions;
            count++;
        }
        line = strtok(NULL, "\n");
    }
    free(raw);
    return count;
}

static int check_suspicious_transmitters(char *report, size_t rlen) {
    report[0] = 0;
    int found = 0;
    pkg_sessions_t current[64];
    int count = get_package_sessions(current, 64);

    for (int i = 0; i < count; i++) {
        /* Find baseline entry */
        int baseline_sessions = 0;
        for (int j = 0; j < g_baseline_session_count; j++) {
            if (g_baseline_sessions[j].uid == current[i].uid) {
                baseline_sessions = g_baseline_sessions[j].sessions;
                break;
            }
        }
        int delta = current[i].sessions - baseline_sessions;
        if (delta > 0) {
            char entry[256];
            snprintf(entry, sizeof(entry), "%s(+%d) ", current[i].pkg, delta);
            if (strlen(report) + strlen(entry) < rlen - 1)
                strncat(report, entry, rlen - strlen(report) - 1);
            found++;
            /* Flag known Xiaomi telemetry packages */
            for (int k = 0; SUSPICIOUS_IDLE_PKGS[k]; k++) {
                if (strstr(current[i].pkg, SUSPICIOUS_IDLE_PKGS[k])) {
                    char msg[256];
                    snprintf(msg, sizeof(msg),
                        "Xiaomi telemetry active while idle: %s +%d sessions",
                        current[i].pkg, delta);
                    tlog("WARN", msg);
                    gaveld_emit("nulld", "XIAOMI_IDLE_TELEMETRY", (double)delta, msg);
                }
            }
        }
    }
    return found;
}

/* ── Write JSON ───────────────────────────────────────────────── */
static void write_json(screen_state_t screen, int tcp4, int tcp6,
                       long rx, long tx, int spike, int idle_secs,
                       long idle_rx_delta, long idle_tx_delta,
                       const char *suspicious, int total_events) {
    FILE *f = results_open("nulld", RESULTS_FILE);
    if (!f) return;
    char ts[32]; time_t t = time(NULL);
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", localtime(&t));

    const char *screen_str = screen == SCREEN_ON  ? "on"  :
                             screen == SCREEN_OFF ? "off" : "unknown";

    fprintf(f,
        "{\n"
        "  \"daemon\": \"nulld\",\n"
        "  \"version\": \"1.1\",\n"
        "  \"timestamp\": \"%s\",\n"
        "  \"screen\": \"%s\",\n"
        "  \"idle_seconds\": %d,\n"
        "  \"connections\": {\n"
        "    \"tcp4\": %d,\n"
        "    \"tcp6\": %d\n"
        "  },\n"
        "  \"traffic\": {\n"
        "    \"rx_bytes\": %ld,\n"
        "    \"tx_bytes\": %ld,\n"
        "    \"idle_rx_delta\": %ld,\n"
        "    \"idle_tx_delta\": %ld\n"
        "  },\n"
        "  \"idle_spike_detected\": %s,\n"
        "  \"total_spike_events\": %d,\n"
        "  \"suspicious_transmitters\": \"%s\",\n"
        "  \"sensor_health\": {\n"
        "    \"adb_reachable\": %s,\n"
        "    \"consecutive_failures\": %d\n"
        "  }\n"
        "}\n",
        ts, screen_str, idle_secs,
        tcp4, tcp6, rx, tx,
        idle_rx_delta, idle_tx_delta,
        spike ? "true" : "false",
        total_events,
        suspicious ? suspicious : "",
        g_adb_healthy ? "true" : "false",
        g_consecutive_failures);
    fflush(f); results_close("nulld", RESULTS_FILE, f);
}

/* g_running declared extern in ipc_globals.h, defined once in
 * ipc_globals.c (defaults to false). Every daemon must set it true
 * before its main loop — nulld's main() previously never did, so the
 * while(g_running) loop below never ran a single iteration regardless
 * of backend/ADB/rish state. Found and fixed 2026-07-11. */
static void handle_sig(int sig) {
    (void)sig;
    g_running = false;
}

int main(void) {
    signal(SIGINT, handle_sig);
    signal(SIGTERM, handle_sig);

    bexec_init();

    tlog("INFO", "nulld v1.1 -- idle transmission monitor online");
    tlog("INFO", "Watching what Xiaomi does while you sleep");

    FILE *pf = fopen(PID_FILE, "w");
    if (pf) { fprintf(pf, "%d\n", getpid()); fclose(pf); }

    g_running = true;

    long baseline_rx = 0, baseline_tx = 0;

    while (g_running) {
        check_adb_health();
        g_screen = get_screen_state();

        int tcp4 = 0, tcp6 = 0;
        get_tcp_counts(&tcp4, &tcp6);

        long rx = 0, tx = 0;
        get_net_bytes(&rx, &tx);

        int spike = 0;
        long idle_rx_delta = 0, idle_tx_delta = 0;
        char suspicious[512] = "";

        /* Screen just turned off — take baseline */
        if (g_screen == SCREEN_OFF && g_prev_screen == SCREEN_ON) {
            g_idle_baseline.tcp4 = tcp4;
            g_idle_baseline.tcp6 = tcp6;
            g_idle_baseline.rx_bytes = rx;
            g_idle_baseline.tx_bytes = tx;
            g_idle_baseline.ts = time(NULL);
            g_idle_secs = 0;
            baseline_rx = rx;
            baseline_tx = tx;
            tlog("INFO", "Screen off — idle baseline captured");
            char msg[128];
            snprintf(msg, sizeof(msg),
                "idle baseline: tcp4=%d tcp6=%d rx=%ldB tx=%ldB",
                tcp4, tcp6, rx, tx);
            tlog("INFO", msg);
        }

        /* Screen is off — monitor for spikes */
        if (g_screen == SCREEN_OFF) {
            g_idle_secs += POLL_SEC;
            idle_rx_delta = rx - baseline_rx;
            idle_tx_delta = tx - baseline_tx;

            int tcp_delta = (tcp4 + tcp6) -
                            (g_idle_baseline.tcp4 + g_idle_baseline.tcp6);

            /* Connection spike while idle */
            if (tcp_delta >= IDLE_SPIKE_THRESHOLD) {
                spike = 1;
                g_spike_count++;
                g_total_events++;
                char msg[256];
                snprintf(msg, sizeof(msg),
                    "CONNECTION SPIKE while idle: +%d connections "
                    "(tcp4=%d tcp6=%d) after %ds idle",
                    tcp_delta, tcp4, tcp6, g_idle_secs);
                tlog("WARN", msg);
                gaveld_emit("nulld", "IDLE_CONNECTION_SPIKE",
                            (double)tcp_delta, msg);
            }

            /* Data transmitted while idle */
            if (idle_tx_delta > IDLE_BYTES_THRESHOLD) {
                spike = 1;
                g_total_events++;
                char msg[256];
                snprintf(msg, sizeof(msg),
                    "DATA TRANSMITTED while idle: %ldKB tx, %ldKB rx "
                    "after %ds idle",
                    idle_tx_delta/1024, idle_rx_delta/1024, g_idle_secs);
                tlog("WARN", msg);
                gaveld_emit("nulld", "IDLE_DATA_TRANSMISSION",
                            (double)(idle_tx_delta/1024), msg);
            }

            /* Check suspicious transmitters every 60s */
            if (g_idle_secs % 60 == 0 && g_idle_secs > 0) {
                check_suspicious_transmitters(suspicious, sizeof(suspicious));
                if (strlen(suspicious) > 0) {
                    char msg[512];
                    snprintf(msg, sizeof(msg),
                        "Suspicious idle transmitters: %s", suspicious);
                    tlog("WARN", msg);
                    gaveld_emit("nulld", "SUSPICIOUS_IDLE_TRANSMITTER",
                                1.0, suspicious);
                }
            }

            char logmsg[256];
            snprintf(logmsg, sizeof(logmsg),
                "idle=%ds tcp4=%d tcp6=%d tx_delta=%ldKB rx_delta=%ldKB",
                g_idle_secs, tcp4, tcp6,
                idle_tx_delta/1024, idle_rx_delta/1024);
            tlog("INFO", logmsg);
        }

        /* Screen just turned on */
        if (g_screen == SCREEN_ON && g_prev_screen == SCREEN_OFF) {
            char msg[256];
            snprintf(msg, sizeof(msg),
                "Screen on after %ds idle — total tx during idle: %ldKB",
                g_idle_secs, (tx - baseline_tx)/1024);
            tlog("INFO", msg);
            g_idle_secs = 0;
        }

        write_json(g_screen, tcp4, tcp6, rx, tx, spike,
                   g_idle_secs, idle_rx_delta, idle_tx_delta,
                   suspicious, g_total_events);

        g_prev_screen = g_screen;
        for (int i = 0; i < POLL_SEC && g_running; i++) sleep(1);
    }

    tlog("INFO", "nulld shutdown");
    unlink(PID_FILE);
    return 0;
}
