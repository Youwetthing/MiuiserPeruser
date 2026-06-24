/*
 * tigerclawd.c v1.1 — Xiaomi Device Integrity & Topology Monitor
 *
 * Universal Xiaomi eye — works across HyperOS/MIUI, Snapdragon/MTK/Exynos.
 * Learns the device's normal state and flags deviations.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <stdint.h>
#include <math.h>

#include "ipc_globals.h"
#include "gaveld_emit.h"

#define BASE           "/data/data/com.termux/files/home/MiuiserPeruser"
#define RESULTS_FILE    BASE "/Registry/daemon_results/tigerclawd.json"
#define BASELINE_FILE   BASE "/data/tigerclawd_baseline.json"
#define PID_FILE        BASE "/pipes/pids/tigerclawd.pid"
#define RISH            "/data/data/com.termux/files/home/Rish/rish"
#define ADB             "/data/data/com.termux/files/home/.cargo/bin/adb_cli"
#define DEFAULT_POLL    30
#define MAX_ANOMALIES   16

static volatile int tc_running = 1;
static int g_poll_sec = DEFAULT_POLL;
static int g_baseline_svc = 0;
static uint32_t g_baseline_prop_hash = 0;
static int g_baseline_established = 0;

typedef struct {
    char type[16];      /* URGENT, WARNING, INFO */
    char code[32];      /* anomaly code */
    char detail[256];   /* human readable */
} Anomaly;

static Anomaly g_anomalies[MAX_ANOMALIES];
static int g_anomaly_count = 0;

static void handle_sig(int s) { (void)s; tc_running = 0; }

static void tlog(const char *lvl, const char *msg) {
    char ts[32];
    time_t t = time(NULL);
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", localtime(&t));
    fprintf(stderr, "[%s][TIGERCLAWD/%s] %s\n", ts, lvl, msg);
    fflush(stderr);
}

static void add_anomaly(const char *type, const char *code, const char *detail) {
    if (g_anomaly_count >= MAX_ANOMALIES) return;
    Anomaly *a = &g_anomalies[g_anomaly_count++];
    strncpy(a->type, type, sizeof(a->type)-1);
    strncpy(a->code, code, sizeof(a->code)-1);
    strncpy(a->detail, detail, sizeof(a->detail)-1);
    tlog(type, detail);
    gaveld_emit("tigerclawd", code, 1.0, detail);
}

static void clear_anomalies(void) {
    g_anomaly_count = 0;
}

/* Run via rish first, adb_cli fallback */
static char *tc_run(const char *cmd) {
    char full[2048];
    snprintf(full, sizeof(full),
        "RISH_APPLICATION_ID=com.termux %s -c '%s' 2>/dev/null", RISH, cmd);
    FILE *fp = popen(full, "r");
    if (fp) {
        char *buf = malloc(65536);
        if (!buf) { pclose(fp); goto try_adb; }
        size_t tot = 0; char tmp[1024]; size_t n;
        while ((n = fread(tmp, 1, sizeof(tmp), fp)) > 0) {
            if (tot + n >= 65535) break;
            memcpy(buf + tot, tmp, n); tot += n;
        }
        pclose(fp);
        buf[tot] = 0;
        if (tot > 2) return buf;
        free(buf);
    }

try_adb:
    snprintf(full, sizeof(full),
        "%s tcp 127.0.0.1:5555 shell \"%s\" 2>/dev/null", ADB, cmd);
    fp = popen(full, "r");
    if (!fp) return NULL;
    char *buf = malloc(65536);
    if (!buf) { pclose(fp); return NULL; }
    size_t tot = 0; char tmp[1024]; size_t n;
    while ((n = fread(tmp, 1, sizeof(tmp), fp)) > 0) {
        if (tot + n >= 65535) break;
        memcpy(buf + tot, tmp, n); tot += n;
    }
    pclose(fp);
    buf[tot] = 0;
    if (!tot) { free(buf); return NULL; }
    return buf;
}

static int count_lines(const char *s) {
    if (!s) return 0;
    int n = 0;
    for (; *s; s++) if (*s == '\n') n++;
    return n;
}

static uint32_t djb2(const char *s) {
    uint32_t h = 5381;
    for (; s && *s; s++) h = ((h << 5) + h) + (unsigned char)*s;
    return h;
}

/* ── Device fingerprinting ───────────────────────────────────── */
static void get_device_info(char *codename, char *board, char *hardware,
                            char *hyperos, char *sec_patch, char *bootloader) {
    char *v;
    v = tc_run("getprop ro.product.device 2>/dev/null");
    if (v) { strncpy(codename, v, 31); codename[strcspn(codename, "\n\r")] = 0; free(v); }
    else strcpy(codename, "unknown");

    v = tc_run("getprop ro.product.board 2>/dev/null");
    if (v) { strncpy(board, v, 31); board[strcspn(board, "\n\r")] = 0; free(v); }
    else strcpy(board, "unknown");

    v = tc_run("getprop ro.hardware 2>/dev/null");
    if (v) { strncpy(hardware, v, 31); hardware[strcspn(hardware, "\n\r")] = 0; free(v); }
    else strcpy(hardware, "unknown");

    v = tc_run("getprop ro.miui.ui.version.name 2>/dev/null");
    if (v) { strncpy(hyperos, v, 31); hyperos[strcspn(hyperos, "\n\r")] = 0; free(v); }
    else {
        v = tc_run("getprop ro.build.version.hyperos 2>/dev/null");
        if (v) { strncpy(hyperos, v, 31); hyperos[strcspn(hyperos, "\n\r")] = 0; free(v); }
        else strcpy(hyperos, "unknown");
    }

    v = tc_run("getprop ro.build.version.security_patch 2>/dev/null");
    if (v) { strncpy(sec_patch, v, 31); sec_patch[strcspn(sec_patch, "\n\r")] = 0; free(v); }
    else strcpy(sec_patch, "unknown");

    v = tc_run("getprop ro.boot.verifiedbootstate 2>/dev/null");
    if (v) { strncpy(bootloader, v, 31); bootloader[strcspn(bootloader, "\n\r")] = 0; free(v); }
    else strcpy(bootloader, "unknown");
}

/* ── Check 1: Suspicious services ───────────────────────────── */
static int check_suspicious_services(const char *services) {
    const char *bad[] = {
        "frida", "xposed", "inject", "magisk", "supersu",
        "substrate", "lspatch", "hook", "edxp", "taichi", NULL
    };
    int found = 0;
    for (int i = 0; bad[i]; i++) {
        if (services && strstr(services, bad[i])) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Suspicious binder service: %s", bad[i]);
            add_anomaly("URGENT", "SUSPICIOUS_BINDER_SERVICE", msg);
            found++;
        }
    }
    return found;
}

/* ── Check 2: SELinux ───────────────────────────────────────── */
static int check_selinux(void) {
    char *mode = tc_run("getenforce 2>/dev/null");
    int enforcing = 1;
    if (mode) {
        if (strstr(mode, "Permissive")) {
            add_anomaly("URGENT", "SELINUX_PERMISSIVE",
                        "SELinux is PERMISSIVE — enforcement disabled");
            enforcing = 0;
        }
        free(mode);
    }
    return enforcing;
}

/* ── Check 3: Debug properties ──────────────────────────────── */
static int check_debug_props(void) {
    int clean = 1;
    struct { const char *prop; const char *safe; } checks[] = {
        { "ro.debuggable", "0" },
        { "ro.secure", "1" },
        { "ro.build.type", "user" },
        { NULL, NULL }
    };
    for (int i = 0; checks[i].prop; i++) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "getprop %s 2>/dev/null", checks[i].prop);
        char *v = tc_run(cmd);
        if (v) {
            v[strcspn(v, "\n\r")] = 0;
            if (strlen(v) > 0 && strcmp(v, checks[i].safe) != 0) {
                char msg[256];
                snprintf(msg, sizeof(msg), "%s=%s (expected %s)",
                         checks[i].prop, v, checks[i].safe);
                add_anomaly("URGENT", "DEBUG_PROP_ANOMALY", msg);
                clean = 0;
            }
            free(v);
        }
    }
    return clean;
}

/* ── Check 4: Security Center ─────────────────────────────────── */
static int check_security_center(void) {
    char *pm = tc_run("pm path com.miui.securitycenter 2>/dev/null");
    int installed = (pm && strstr(pm, "package:")) ? 1 : 0;
    free(pm);
    if (!installed) {
        add_anomaly("WARNING", "SECURITY_CENTER_MISSING",
                    "MIUI Security Center not installed");
        return 0;
    }

    char *svc = tc_run("service list 2>/dev/null | grep -c securitycenter");
    int running = svc ? (atoi(svc) > 0) : 0;
    free(svc);
    if (!running) {
        add_anomaly("WARNING", "SECURITY_CENTER_DOWN",
                    "MIUI Security Center installed but not running");
        return 0;
    }
    return 1;
}

/* ── Check 5: Xiaomi core services (soft check) ───────────── */
static int check_xiaomi_services(const char *services) {
    const char *core[] = {
        "MiuiInit", "ProcessManager", "anrrescue", NULL
    };
    int missing = 0;
    for (int i = 0; core[i]; i++) {
        if (!services || !strstr(services, core[i])) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Core Xiaomi service absent: %s", core[i]);
            add_anomaly("INFO", "XIAOMI_SERVICE_MISSING", msg);
            missing++;
        }
    }
    return missing;
}

/* ── Check 6: Developer / ADB state ───────────────────────────── */
static void check_dev_state(int *dev_opts, int *adb_enabled, int *adb_net) {
    char *v = tc_run("getprop ro.debuggable 2>/dev/null");
    *dev_opts = (v && strcmp(v, "0\n") != 0 && strcmp(v, "0\r\n") != 0) ? 1 : 0;
    free(v);

    v = tc_run("getprop sys.usb.state 2>/dev/null");
    *adb_enabled = (v && strstr(v, "adb")) ? 1 : 0;
    free(v);

    v = tc_run("getprop persist.adb.tcp.port 2>/dev/null");
    *adb_net = (v && atoi(v) > 0) ? 1 : 0;
    free(v);

    if (*dev_opts) add_anomaly("WARNING", "DEV_OPTIONS_ENABLED",
                                "Developer options enabled (ro.debuggable != 0)");
    if (*adb_net) add_anomaly("WARNING", "ADB_OVER_NETWORK",
                               "ADB over network enabled");
}

/* ── Check 7: Property hash ───────────────────────────────────── */
static uint32_t get_prop_hash(void) {
    char *props = tc_run("getprop 2>/dev/null | grep -E '^\\[ro\\.' | sort");
    uint32_t h = djb2(props);
    free(props);
    return h;
}

/* ── Baseline ─────────────────────────────────────────────────── */
static void save_baseline(int svc_count, uint32_t prop_hash) {
    FILE *f = fopen(BASELINE_FILE, "w");
    if (!f) return;
    fprintf(f, "{\"service_count\":%d,\"prop_hash\":%u,\"established_at\":%ld}\n",
            svc_count, prop_hash, (long)time(NULL));
    fclose(f);
    g_baseline_svc = svc_count;
    g_baseline_prop_hash = prop_hash;
    g_baseline_established = 1;
    tlog("INFO", "Baseline established");
}

static void load_baseline(void) {
    FILE *f = fopen(BASELINE_FILE, "r");
    if (!f) return;
    char buf[256]; fgets(buf, sizeof(buf), f); fclose(f);
    char *p = strstr(buf, "\"service_count\":");
    if (p) g_baseline_svc = atoi(p + 16);
    p = strstr(buf, "\"prop_hash\":");
    if (p) g_baseline_prop_hash = (uint32_t)strtoul(p + 12, NULL, 10);
    g_baseline_established = (g_baseline_svc > 0);
    if (g_baseline_established) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Baseline loaded: %d services", g_baseline_svc);
        tlog("INFO", msg);
    }
}

/* ── Trust score ──────────────────────────────────────────────── */
static int calc_trust_score(int selinux, int debug_clean, int sc_running,
                            int dev_opts, int adb_net, int suspicious) {
    int score = 100;
    if (!selinux) score -= 25;
    if (!debug_clean) score -= 20;
    if (!sc_running) score -= 15;
    if (dev_opts) score -= 10;
    if (adb_net) score -= 10;
    score -= (suspicious * 15);
    if (score < 0) score = 0;
    return score;
}

/* ── JSON writer ──────────────────────────────────────────────── */
static void write_json(int svc_count, int svc_drift, int suspicious,
                       int selinux, int debug_clean, int sc_running,
                       int dev_opts, int adb_enabled, int adb_net,
                       int prop_drift, int trust_score,
                       const char *codename, const char *board,
                       const char *hardware, const char *hyperos,
                       const char *sec_patch, const char *bootloader,
                       int ms) {
    FILE *f = fopen(RESULTS_FILE, "w");
    if (!f) return;
    char ts[32]; time_t t = time(NULL);
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", localtime(&t));

    fprintf(f,
        "{\n"
        "  \"daemon\": \"tigerclawd\",\n"
        "  \"version\": \"1.1\",\n"
        "  \"timestamp\": \"%s\",\n"
        "  \"poll_interval_sec\": %d,\n"
        "  \"device\": {\n"
        "    \"codename\": \"%s\",\n"
        "    \"board\": \"%s\",\n"
        "    \"hardware\": \"%s\",\n"
        "    \"hyperos_version\": \"%s\",\n"
        "    \"security_patch\": \"%s\",\n"
        "    \"bootloader_locked\": %s\n"
        "  },\n"
        "  \"binder\": {\n"
        "    \"service_count\": %d,\n"
        "    \"baseline_count\": %d,\n"
        "    \"drift\": %d,\n"
        "    \"suspicious_services\": %d\n"
        "  },\n"
        "  \"integrity\": {\n"
        "    \"selinux_enforcing\": %s,\n"
        "    \"debug_props_clean\": %s,\n"
        "    \"security_center_running\": %s,\n"
        "    \"developer_options\": %s,\n"
        "    \"adb_enabled\": %s,\n"
        "    \"adb_over_network\": %s,\n"
        "    \"prop_hash_drift\": %s\n"
        "  },\n"
        "  \"trust_score\": %d,\n"
        "  \"anomalies\": [\n",
        ts, g_poll_sec,
        codename, board, hardware, hyperos, sec_patch,
        (strcmp(bootloader, "green") == 0 || strcmp(bootloader, "locked") == 0) ? "true" : "false",
        svc_count, g_baseline_svc, svc_drift, suspicious,
        selinux ? "true" : "false",
        debug_clean ? "true" : "false",
        sc_running ? "true" : "false",
        dev_opts ? "true" : "false",
        adb_enabled ? "true" : "false",
        adb_net ? "true" : "false",
        prop_drift ? "true" : "false",
        trust_score);

    for (int i = 0; i < g_anomaly_count; i++) {
        fprintf(f,
            "    {\n"
            "      \"type\": \"%s\",\n"
            "      \"code\": \"%s\",\n"
            "      \"detail\": \"%s\"\n"
            "    }%s\n",
            g_anomalies[i].type, g_anomalies[i].code, g_anomalies[i].detail,
            (i < g_anomaly_count - 1) ? "," : "");
    }

    fprintf(f,
        "  ],\n"
        "  \"poll_duration_ms\": %d\n"
        "}\n", ms);

    fflush(f); fclose(f);
}

int main(void) {
    signal(SIGINT,  handle_sig);
    signal(SIGTERM, handle_sig);
    tlog("INFO", "tigerclawd v1.1 — Xiaomi eye online");

    FILE *pf = fopen(PID_FILE, "w");
    if (pf) { fprintf(pf, "%d\n", getpid()); fclose(pf); }

    load_baseline();

    while (tc_running) {
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        clear_anomalies();

        /* Device fingerprint */
        char codename[32] = "unknown", board[32] = "unknown";
        char hardware[32] = "unknown", hyperos[32] = "unknown";
        char sec_patch[32] = "unknown", bootloader[32] = "unknown";
        get_device_info(codename, board, hardware, hyperos, sec_patch, bootloader);

        /* Service list */
        char *services = tc_run("service list 2>/dev/null");
        int svc_count = count_lines(services);

        /* Establish baseline */
        if (!g_baseline_established && svc_count > 10) {
            uint32_t ph = get_prop_hash();
            save_baseline(svc_count, ph);
        }

        /* Run checks */
        int suspicious = check_suspicious_services(services);
        int svc_missing = check_xiaomi_services(services);
        int selinux = check_selinux();
        int debug_clean = check_debug_props();
        int sc_running = check_security_center();

        int dev_opts = 0, adb_enabled = 0, adb_net = 0;
        check_dev_state(&dev_opts, &adb_enabled, &adb_net);

        /* Service drift */
        int svc_drift = 0;
        if (g_baseline_established && svc_count > 0) {
            svc_drift = svc_count - g_baseline_svc;
            if (abs(svc_drift) > 5) {
                char msg[128];
                snprintf(msg, sizeof(msg),
                    "Binder drift: %+d services (baseline %d, now %d)",
                    svc_drift, g_baseline_svc, svc_count);
                add_anomaly("WARNING", "BINDER_TOPOLOGY_DRIFT", msg);
            }
        }

        /* Property hash drift */
        int prop_drift = 0;
        if (g_baseline_established) {
            uint32_t ph = get_prop_hash();
            if (ph != g_baseline_prop_hash && g_baseline_prop_hash != 0) {
                add_anomaly("WARNING", "PROP_HASH_DRIFT",
                            "Read-only property set changed since baseline");
                prop_drift = 1;
                g_baseline_prop_hash = ph;
            }
        }

        int trust_score = calc_trust_score(selinux, debug_clean, sc_running,
                                           dev_opts, adb_net, suspicious);

        clock_gettime(CLOCK_MONOTONIC, &t1);
        int ms = (int)((t1.tv_sec - t0.tv_sec) * 1000 +
                       (t1.tv_nsec - t0.tv_nsec) / 1000000);

        write_json(svc_count, svc_drift, suspicious, selinux, debug_clean,
                   sc_running, dev_opts, adb_enabled, adb_net, prop_drift,
                   trust_score, codename, board, hardware, hyperos,
                   sec_patch, bootloader, ms);

        char logmsg[256];
        snprintf(logmsg, sizeof(logmsg),
            "services=%d drift=%+d score=%d/%d anomalies=%d dur=%dms",
            svc_count, svc_drift, trust_score, 100, g_anomaly_count, ms);
        tlog("INFO", logmsg);

        free(services);
        for (int i = 0; i < g_poll_sec && tc_running; i++) sleep(1);
    }

    tlog("INFO", "tigerclawd shutdown");
    unlink(PID_FILE);
    return 0;
}
