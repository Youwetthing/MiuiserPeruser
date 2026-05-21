/*
 * burned.c — MIUI / HyperOS Policy, Privacy & Identity Guardian
 *
 * Domain: MIUI/HyperOS system properties
 *   - Identity fingerprint (OS version, region, locale)
 *   - Privacy / tracking: partner IDs baked into ROM, preinstalled SDKs
 *   - Policy: optimization flags, process management aggressiveness,
 *             telemetry consent, performance tuning
 *
 * NOT in this daemon's domain:
 *   - ADB / USB security       → granitord
 *   - SUPL / GPS network       → rahzerd
 *   - Cloud sync / network     → rahzerd
 *   - Boot integrity / SELinux → granitord
 *
 * Two-pass scan each poll:
 *   Pass 1 — binary prop table  (invasive_if_1 flag)
 *   Pass 2 — privacy prop table (string-match / non-empty checks)
 *
 * Signals emitted to Gaveld + Splinterd:
 *   HYPEROS_DETECTED, EEA_BUILD, IMEI_EXPOSED, FACEBOOK_PARTNER_BAKED,
 *   APPSFLYER_PREINSTALL, PARTNER_BLOATWARE, DEVICE_SERIAL_EXPOSED,
 *   GOOGLE_CLIENT_XIAOMI, MSA_TELEMETRY_ACTIVE, MISIGHT_ANALYTICS_ON,
 *   MIUI_OPTIMIZATION_OFF, MIUI_RESTRICTED_MODE, POWERKEEPER_ACTIVE,
 *   GAME_TURBO_ACTIVE, CLEANER_AGGRESSIVE, RAM_EXTENSION_ACTIVE,
 *   MIUI_BOOSTER_RTMODE, PROCESS_KILL_AGGRESSIVE, APP_DOWNGRADE_ACTIVE,
 *   SMART_GC_AGGRESSIVE
 *
 * Runtime config (Registry/daemon_state.json):
 *   burned.enabled   — 0 = exit, 1 = run
 *   burned.interval  — seconds between polls (default 60)
 *   burned.scan_count— max polls before exit, 0 = unlimited
 */

#include "ipc_globals.h"
#include "backend_exec.h"
#include "gaveld_emit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>

#define DAEMON_NAME      "burned"
#define DEFAULT_INTERVAL 60
#define MAX_BINARY_PROPS 24
#define MAX_PRIV_PROPS   16

#ifndef MP_BASE_DIR
#define MP_BASE_DIR "/data/data/com.termux/files/home/MiuiserPeruser"
#endif

#define STATE_FILE   MP_BASE_DIR "/Registry/daemon_state.json"
#define RESULTS_DIR  MP_BASE_DIR "/Registry/daemon_results"
#define RESULTS_FILE RESULTS_DIR "/" DAEMON_NAME ".json"

/* ── Config ───────────────────────────────────────────────────────────── */

static int config_int(const char *key, int def)
{
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "jq -r '.%s.%s // %d' %s 2>/dev/null",
             DAEMON_NAME, key, def, STATE_FILE);
    FILE *f = popen(cmd, "r");
    if (!f) return def;
    char buf[32] = {0};
    int  val = def;
    if (fgets(buf, sizeof(buf), f) && buf[0] != 'n')
        val = atoi(buf);
    pclose(f);
    return val;
}

static int is_enabled(void)    { return config_int("enabled",    1); }
static int get_interval(void)  { return config_int("interval",   DEFAULT_INTERVAL); }
static int get_max_scans(void) { return config_int("scan_count", 0); }

/* ── IPC helpers ──────────────────────────────────────────────────────── */

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

/* ── Getprop ──────────────────────────────────────────────────────────── */

static void getprop_val(const char *key, char *out, size_t len)
{
    char cmd[160];
    snprintf(cmd, sizeof(cmd), "getprop '%s' 2>/dev/null", key);
    FILE *f = popen(cmd, "r");
    if (!f) { strncpy(out, "(err)", len); return; }
    out[0] = '\0';
    fgets(out, (int)len, f);
    pclose(f);
    out[strcspn(out, "\n\r")] = '\0';
    if (!out[0]) strncpy(out, "(unset)", len);
}

/* ── Pass 1: binary prop table ────────────────────────────────────────── */
/*
 * invasive_if_1 : signal fires when value == "1"
 * invasive_if_0 : signal fires when value == "0" (miui_optimization)
 * signal        : emitted to gaveld/splinterd when condition met
 */
typedef struct {
    const char *key;
    const char *label;
    const char *signal;       /* NULL = no gaveld emit, just log */
    int         invasive_if_1;/* 1 = fire on "1", -1 = fire on "0" */
    char        cur[80];
    char        prev[80];
} bprop_t;

static bprop_t g_bprops[MAX_BINARY_PROPS] = {
    /* MIUI/HyperOS policy flags */
    { "persist.sys.miui_optimization",          "MIUI Optimisation",     "MIUI_OPTIMIZATION_OFF",    -1, {0},{0} },
    { "persist.sys.miui_restricted_mode",       "Restricted Mode",       "MIUI_RESTRICTED_MODE",      1, {0},{0} },
    { "persist.sys.powerkeeper",                "PowerKeeper",           "POWERKEEPER_ACTIVE",         1, {0},{0} },
    { "persist.sys.game_turbo_enabled",         "Game Turbo",            "GAME_TURBO_ACTIVE",          1, {0},{0} },
    { "persist.sys.memory_extension_enabled",   "RAM Extension",         "RAM_EXTENSION_ACTIVE",       1, {0},{0} },
    { "persist.sys.miui.turbosched",            "TurboSched",            NULL,                         1, {0},{0} },
    { "persist.sys.perfshielder",               "PerfShielder",          NULL,                         1, {0},{0} },
    { "miui.whetstone.power",                   "Whetstone Power",       NULL,                         1, {0},{0} },
    /* Analytics / telemetry */
    { "persist.sys.miui.msa.consent",           "MSA Telemetry",         "MSA_TELEMETRY_ACTIVE",       1, {0},{0} },
    { "persist.sys.background_data",            "Background Data",       NULL,                         1, {0},{0} },
    /* Performance / process management */
    { "persist.sys.miuibooster.rtmode",         "Booster RTMode",        "MIUI_BOOSTER_RTMODE",        1, {0},{0} },
    { "persist.sys.miui_scout_binder_full_kill_process","Binder Full Kill","PROCESS_KILL_AGGRESSIVE",  1, {0},{0} },
    { "persist.sys.smart_gc.enable",            "Smart GC",              "SMART_GC_AGGRESSIVE",        1, {0},{0} },
    { "persist.sys.lmk.reportkills",            "LMK Kill Report",       NULL,                         1, {0},{0} },
    { "persist.sys.perf_scenario_recognition.enable","Perf Scenario",    NULL,                         1, {0},{0} },
    { "persist.sys.enable_rtmode_uclamp",       "RT Uclamp",             NULL,                         1, {0},{0} },
    /* Stability */
    { "persist.sys.stability.window_monitor.kill_when_leak","Kill On Leak",NULL,                       1, {0},{0} },
    { "persist.sys.disable_restart_threshold",  "Restart Threshold",     NULL,                         1, {0},{0} },
    /* Informational */
    { "persist.sys.autostart",                  "Autostart Control",     NULL,                         0, {0},{0} },
    { "persist.sys.performance_mode",           "Perf Mode",             NULL,                         1, {0},{0} },
};
static const int g_nbprops = 20;

/* ── Pass 2: privacy prop table ───────────────────────────────────────── */
/*
 * Fires when value matches condition:
 *   match == NULL   → fires if value != "(unset)" and != "(err)"
 *   match != NULL   → fires if value contains match (or != match if negate)
 */
typedef struct {
    const char *key;
    const char *label;
    const char *signal;
    const char *match;  /* NULL = any non-empty value fires */
    int         negate; /* 1 = fire when value does NOT match */
    char        cur[80];
} pprop_t;

static pprop_t g_pprops[MAX_PRIV_PROPS] = {
    /* Partner IDs baked into ROM — fire if non-empty */
    { "persist.sys.facebook.partnerid",  "FB Partner ID (persist)",  "FACEBOOK_PARTNER_BAKED",  NULL,     0, {0} },
    { "ro.facebook.partnerid",           "FB Partner ID (ro)",       "FACEBOOK_PARTNER_BAKED",  NULL,     0, {0} },
    { "ro.appsflyer.preinstall.path",    "AppsFlyer Preinstall",     "APPSFLYER_PREINSTALL",    NULL,     0, {0} },
    { "ro.csc.spotify.music.partnerid",  "Spotify Partner",          "PARTNER_BLOATWARE",       NULL,     0, {0} },
    { "persist.sys.netflix.channel",     "Netflix Channel",          "PARTNER_BLOATWARE",       NULL,     0, {0} },
    { "ro.com.google.clientidbase",      "Google Client Base",       "GOOGLE_CLIENT_XIAOMI",    "android-xiaomi", 0, {0} },
    /* Device identity exposure */
    { "persist.sys.miui.sno",           "Device SNO",               "DEVICE_SERIAL_EXPOSED",   NULL,     0, {0} },
    { "ro.ril.miui.imei0",              "IMEI0 (ro)",               "IMEI_EXPOSED",            NULL,     0, {0} },
    { "ro.ril.miui.imei1",              "IMEI1 (ro)",               "IMEI_EXPOSED",            NULL,     0, {0} },
    /* MiSight analytics — fire if NOT "off" (negate=1, match="off") */
    { "persist.sys.misight.ue_mode",    "MiSight UE Mode",          "MISIGHT_ANALYTICS_ON",    "off",    1, {0} },
    /* App downgrade policy — fire if value is a number > 0 */
    { "persist.sys.downgrade_after_inactive_days","App Downgrade Days","APP_DOWNGRADE_ACTIVE",  "(unset)",1, {0} },
    /* Cleaner level — fire if >= 2 */
    { "persist.sys.cleaner_level",      "Cleaner Level",            "CLEANER_AGGRESSIVE",      NULL,     0, {0} },
    /* BG app limit — informational */
    { "persist.sys.mms.bg_apps_limit",  "BG App Limit",             NULL,                      NULL,     0, {0} },
    /* Perf turbo type — informational */
    { "persist.sys.perf_turbo_type",    "Perf Turbo Type",          NULL,                      NULL,     0, {0} },
    /* Stability: reboot policy */
    { "persist.sys.stability.reboot_days","Forced Reboot Days",     "FORCED_REBOOT_POLICY",   "(unset)", 1, {0} },
    /* OS identity */
    { "ro.mi.os.version.name",          "OS Version",               "HYPEROS_DETECTED",        "OS",     0, {0} },
};
static const int g_npprops = 16;

/* ── Utility: check if prop fires ────────────────────────────────────── */

static int pprop_fires(const pprop_t *p)
{
    const char *v = p->cur;
    if (strcmp(v, "(err)") == 0) return 0;

    if (!p->match) {
        /* fire if non-empty / unset */
        int set = (strcmp(v, "(unset)") != 0);
        return p->negate ? !set : set;
    }
    int contains = (strstr(v, p->match) != NULL);
    return p->negate ? !contains : contains;
}

/* ── Results ──────────────────────────────────────────────────────────── */

static void write_results(int n_invasive, int n_privacy, int n_change,
                           const char *inv_list, const char *priv_list,
                           int scan_num)
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
        "  \"invasive_policy_count\": %d,\n"
        "  \"privacy_signal_count\": %d,\n"
        "  \"change_count\": %d,\n"
        "  \"invasive_list\": \"%s\",\n"
        "  \"privacy_list\": \"%s\",\n"
        "  \"binary_props\": [\n",
        ts, scan_num, n_invasive, n_privacy, n_change,
        inv_list, priv_list);

    for (int i = 0; i < g_nbprops; i++) {
        fprintf(f, "    {\"key\":\"%s\",\"label\":\"%s\",\"value\":\"%s\"}%s\n",
                g_bprops[i].key, g_bprops[i].label, g_bprops[i].cur,
                i < g_nbprops - 1 ? "," : "");
    }
    fprintf(f, "  ],\n  \"privacy_props\": [\n");
    for (int i = 0; i < g_npprops; i++) {
        fprintf(f, "    {\"key\":\"%s\",\"label\":\"%s\",\"value\":\"%s\"}%s\n",
                g_pprops[i].key, g_pprops[i].label, g_pprops[i].cur,
                i < g_npprops - 1 ? "," : "");
    }
    fprintf(f, "  ]\n}\n");
    fclose(f);
}

/* ── Poll ─────────────────────────────────────────────────────────────── */

static int  g_first = 1;
static char g_deduped_signals[512];  /* track signals already emitted this poll */

static void dedup_reset(void) { g_deduped_signals[0] = '\0'; }

static int dedup_check_add(const char *sig)
{
    /* Return 0 if already seen this poll, 1 if new */
    if (strstr(g_deduped_signals, sig)) return 0;
    strncat(g_deduped_signals, sig, sizeof(g_deduped_signals) - strlen(g_deduped_signals) - 2);
    strncat(g_deduped_signals, ",", sizeof(g_deduped_signals) - strlen(g_deduped_signals) - 1);
    return 1;
}

static void emit_signal(const char *sig, const char *detail)
{
    if (!sig || !dedup_check_add(sig)) return;
    gaveld_emit(DAEMON_NAME, sig, 0.0, detail);
    char payload[256];
    snprintf(payload, sizeof(payload), "signal=%s detail=%.200s", sig, detail);
    splinterd_emit("miui_signal", payload);
}

static void poll_burned(int scan_num)
{
    time_t t = time(NULL);
    char ts[32];
    strftime(ts, sizeof(ts), "%H:%M:%S", localtime(&t));

    printf("\n[BURNED] ── MIUI/HyperOS Scan #%d  %s ─────────────────────\n",
           scan_num, ts);

    dedup_reset();

    int n_invasive = 0, n_change = 0, n_privacy = 0;
    char inv_list[256] = {0};
    char priv_list[256] = {0};

    /* ── Pass 1: binary props ────────────────────────────────────────── */
    printf("[BURNED]  [POLICY PROPS]\n");
    printf("[BURNED]  %-32s %-22s %s\n", "Property", "Value", "Status");
    printf("[BURNED]  %s\n", "────────────────────────────────────────────────────────");

    for (int i = 0; i < g_nbprops; i++) {
        bprop_t *p = &g_bprops[i];
        memcpy(p->prev, p->cur, sizeof(p->cur));
        getprop_val(p->key, p->cur, sizeof(p->cur));

        int changed  = (!g_first && strcmp(p->cur, p->prev) != 0);
        int invasive = (p->invasive_if_1 ==  1 && strcmp(p->cur, "1") == 0) ||
                       (p->invasive_if_1 == -1 && strcmp(p->cur, "0") == 0);

        printf("[BURNED]  %-32s %-22s%s%s\n",
               p->label, p->cur,
               invasive ? " [!]" : "",
               changed  ? " CHANGED" : "");

        if (invasive && p->signal) {
            n_invasive++;
            emit_signal(p->signal, p->label);
            if (inv_list[0]) strncat(inv_list, ",", sizeof(inv_list) - strlen(inv_list) - 1);
            strncat(inv_list, p->signal, sizeof(inv_list) - strlen(inv_list) - 1);
        }
        if (changed) n_change++;
    }

    /* ── Pass 2: privacy props ───────────────────────────────────────── */
    printf("\n[BURNED]  [PRIVACY / TRACKING PROPS]\n");
    printf("[BURNED]  %-32s %-22s %s\n", "Property", "Value", "Status");
    printf("[BURNED]  %s\n", "────────────────────────────────────────────────────────");

    for (int i = 0; i < g_npprops; i++) {
        pprop_t *p = &g_pprops[i];
        getprop_val(p->key, p->cur, sizeof(p->cur));
        int fires = pprop_fires(p);

        printf("[BURNED]  %-32s %-22s%s\n",
               p->label, p->cur, fires ? " [!]" : "");

        if (fires && p->signal) {
            n_privacy++;
            emit_signal(p->signal, p->label);
            if (priv_list[0]) strncat(priv_list, ",", sizeof(priv_list) - strlen(priv_list) - 1);
            strncat(priv_list, p->signal, sizeof(priv_list) - strlen(priv_list) - 1);
        }
    }

    printf("\n[BURNED]  Policy flags: %-3d  Privacy signals: %-3d  Changes: %d\n",
           n_invasive, n_privacy, n_change);

    /* Aggregate emit */
    if (n_invasive > 0 || n_privacy > 0) {
        char ctx[512];
        snprintf(ctx, sizeof(ctx),
                 "policy=%d privacy=%d policy_list=%.100s privacy_list=%.100s",
                 n_invasive, n_privacy, inv_list, priv_list);
        gaveld_emit(DAEMON_NAME, "MIUI_POLICY_ACTIVE", (float)(n_invasive + n_privacy), ctx);
        splinterd_emit("miui_policy", ctx);
    }
    if (n_change > 0) {
        char ctx[128];
        snprintf(ctx, sizeof(ctx), "count=%d", n_change);
        gaveld_emit(DAEMON_NAME, "MIUI_PROPERTY_CHANGED", (float)n_change, ctx);
        splinterd_emit("miui_change", ctx);
    }

    write_results(n_invasive, n_privacy, n_change, inv_list, priv_list, scan_num);
    g_first = 0;
}

/* ── Main ─────────────────────────────────────────────────────────────── */

int main(void)
{
    if (!is_enabled()) {
        printf("[BURNED] disabled via syndicatectl — exiting\n");
        return 0;
    }

    printf("[BURNED] MIUI/HyperOS Policy & Privacy Guardian: ONLINE\n");

    int interval  = get_interval();
    int max_scans = get_max_scans();
    int scan_num  = 0;

    printf("[BURNED] interval=%ds  max_scans=%s\n",
           interval, max_scans == 0 ? "unlimited" : "limited");

    for (;;) {
        if (!is_enabled()) {
            printf("[BURNED] disabled — stopping\n");
            break;
        }
        interval  = get_interval();
        max_scans = get_max_scans();

        scan_num++;
        poll_burned(scan_num);

        if (max_scans > 0 && scan_num >= max_scans) {
            printf("[BURNED] reached scan_count=%d — exiting\n", max_scans);
            break;
        }
        sleep(interval);
    }
    return 0;
}
