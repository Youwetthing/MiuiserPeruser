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
 * Three-pass scan each poll:
 *   Pass 1 — binary prop table   (invasive_if_1 flag)
 *   Pass 2 — privacy prop table  (string-match / non-empty checks,
 *            device-identity + policy signals only — region-agnostic)
 *   Pass 3 — partner signal table (loaded from xiaomi_partners.db at
 *            startup, filtered by detected region; region-specific
 *            bloatware / SDK / tracking partner IDs)
 *
 * Region genericization (this version):
 *   Region-specific partner signals used to be hardcoded string
 *   literals in g_pprops (Facebook partner ID, AppsFlyer, Spotify,
 *   Netflix, Google client base) — meaning burned.c only ever matched
 *   what happened to be baked into one particular device's ROM. Those
 *   rows are removed from g_pprops. In their place: detect_region()
 *   reads ro.miui.build.region / ro.rom.zone / ro.product.mod_device
 *   once at startup (same cached-at-startup pattern as shredderd's
 *   check_kernel_config()), and load_partner_db() pulls every partner
 *   signal matching that region (plus 'Global') out of
 *   Registry/xiaomi_partners.db into g_dbprops. burned.c itself now
 *   carries zero region-specific data — the binary is the same on
 *   every Xiaomi device, only the DB content varies. An empty or
 *   missing DB is harmless: zero partner signals load, Pass 1/2 keep
 *   working unaffected.
 *
 *   REGION_MARKER_* / MIUI_BUILD_REGION rows in the partners table are
 *   detection inputs (used by scripts doing external region lookups),
 *   not findings — load_partner_db()'s query explicitly excludes them
 *   so they don't get silently no-op'd through db_pprop_fires().
 *
 * Signals emitted to Gaveld + Splinterd:
 *   HYPEROS_DETECTED, EEA_BUILD, IMEI_EXPOSED,
 *   DEVICE_SERIAL_EXPOSED, MSA_TELEMETRY_ACTIVE, MISIGHT_ANALYTICS_ON,
 *   MIUI_OPTIMIZATION_OFF, MIUI_RESTRICTED_MODE, POWERKEEPER_ACTIVE,
 *   GAME_TURBO_ACTIVE, CLEANER_AGGRESSIVE, RAM_EXTENSION_ACTIVE,
 *   MIUI_BOOSTER_RTMODE, PROCESS_KILL_AGGRESSIVE, APP_DOWNGRADE_ACTIVE,
 *   SMART_GC_AGGRESSIVE, plus whatever partner signal_names are
 *   present in xiaomi_partners.db for the detected region (see Pass 3)
 *
 * Runtime config (Registry/daemon_state.json):
 *   burned.enabled   — 0 = exit, 1 = run
 *   burned.interval  — seconds between polls (default 60)
 *   burned.scan_count— max polls before exit, 0 = unlimited
 *
 * Runtime data:
 *   BURNED_LOG_PATH env var — optional file log destination (same
 *   convention as shredderd's SHREDDER_LOG_PATH); stderr is always
 *   written to regardless.
 */

#include "ipc_globals.h"
#include "gaveld_emit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sqlite3.h>

#define DAEMON_NAME      "burned"
#define DEFAULT_INTERVAL 60
#define MAX_BINARY_PROPS 24
#define MAX_PRIV_PROPS   16
#define MAX_DB_PPROPS    64

#ifndef MP_BASE_DIR
#define MP_BASE_DIR "/data/data/com.termux/files/home/MiuiserPeruser"
#endif

#define STATE_FILE   MP_BASE_DIR "/Registry/daemon_state.json"
#define RESULTS_DIR  MP_BASE_DIR "/Registry/daemon_results"
#define RESULTS_FILE RESULTS_DIR "/" DAEMON_NAME ".json"
#define PARTNER_DB   MP_BASE_DIR "/Registry/xiaomi_partners.db"

/* ── Logging (shredderd-style: stderr always, optional file dest) ──────── */

static FILE *g_burned_log_fp = NULL;

static void burnedlog_init(void)
{
    const char *path = getenv("BURNED_LOG_PATH");
    if (path && *path) {
        g_burned_log_fp = fopen(path, "a");
        if (!g_burned_log_fp) {
            fprintf(stderr, "[BURNED] WARN: cannot open log file %s: %s\n",
                    path, strerror(errno));
        }
    }
}

/* NOTE: uses a separate va_start/va_end per output destination.
 * Reusing one va_list across two vfprintf() calls is undefined
 * behavior — this bit krangd once already; don't repeat it here. */
static void burnedlog(const char *level, const char *fmt, ...)
{
    time_t t = time(NULL);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", localtime(&t));

    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[%s][BURNED/%s] ", ts, level);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);

    if (g_burned_log_fp) {
        va_list ap2;
        va_start(ap2, fmt);
        fprintf(g_burned_log_fp, "[%s][BURNED/%s] ", ts, level);
        vfprintf(g_burned_log_fp, fmt, ap2);
        fprintf(g_burned_log_fp, "\n");
        va_end(ap2);
        fflush(g_burned_log_fp);
    }
}

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

/* ── True/false string helpers (Xiaomi mixes "1"/"0" and "true"/"false") ── */

static int prop_is_true(const char *v)  { return !strcmp(v, "1") || !strcasecmp(v, "true"); }
static int prop_is_false(const char *v) { return !strcmp(v, "0") || !strcasecmp(v, "false"); }

/* ── Region detection ─────────────────────────────────────────────────── */

static char g_region[16] = "Unknown";

static void to_lower_buf(char *s) { for (; *s; s++) *s = (char)tolower((unsigned char)*s); }

static void detect_region(void)
{
    char v[80] = "(unset)";
    char lv[80];

    getprop_val("ro.miui.build.region", v, sizeof(v));
    if (!strcmp(v, "(unset)") || !strcmp(v, "(err)"))
        getprop_val("ro.rom.zone", v, sizeof(v));
    if (!strcmp(v, "(unset)") || !strcmp(v, "(err)"))
        getprop_val("ro.product.mod_device", v, sizeof(v));

    strncpy(lv, v, sizeof(lv) - 1);
    lv[sizeof(lv) - 1] = '\0';
    to_lower_buf(lv);

    if (strstr(lv, "cn"))
        strncpy(g_region, "China", sizeof(g_region) - 1);
    else if (strstr(lv, "eea") || strstr(lv, "eu"))
        strncpy(g_region, "EEA", sizeof(g_region) - 1);
    else if (strstr(lv, "global"))
        strncpy(g_region, "Global", sizeof(g_region) - 1);
    else if (strstr(lv, "in"))
        strncpy(g_region, "India", sizeof(g_region) - 1);
    else
        strncpy(g_region, "Unknown", sizeof(g_region) - 1);
    g_region[sizeof(g_region) - 1] = '\0';

    burnedlog("INFO", "region detected: raw=\"%s\" -> %s", v, g_region);
}

/* ── Pass 3: partner signals loaded from xiaomi_partners.db ────────────── */
/*
 * prop_value in the DB may be a single literal or a comma-separated
 * list (e.g. MIUI_PRECACHE_CHINESE_APPS lists several package names) —
 * db_pprop_fires() treats a comma-list as "fires if the live value
 * contains ANY listed token", single values as substring match.
 */
typedef struct {
    char signal[64];
    char key[80];
    char expect[160];
    int  priority;
    char cur[80];
} db_pprop_t;

static db_pprop_t g_dbprops[MAX_DB_PPROPS];
static int g_ndbprops = 0;

static void load_partner_db(void)
{
    sqlite3 *db = NULL;
    int rc = sqlite3_open_v2(PARTNER_DB, &db, SQLITE_OPEN_READONLY, NULL);
    if (rc != SQLITE_OK) {
        burnedlog("ERROR", "partner db unavailable at %s: %s",
                  PARTNER_DB, db ? sqlite3_errmsg(db) : "open failed");
        if (db) sqlite3_close(db);
        return;
    }

    /* Exclude region-marker/meta rows — those are detection inputs
     * (used by external scripts doing region lookups), not partner
     * findings, and their prop_value isn't a literal match string. */
    const char *sql =
        "SELECT signal_name, prop_key, prop_value, priority FROM partners "
        "WHERE (region = ?1 OR region = 'Global' OR region LIKE '%' || ?1 || '%') "
        "AND signal_name NOT LIKE 'REGION_MARKER%' "
        "AND signal_name != 'MIUI_BUILD_REGION' "
        "ORDER BY priority;";

    sqlite3_stmt *stmt = NULL;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        burnedlog("ERROR", "partner db query prepare failed: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }
    sqlite3_bind_text(stmt, 1, g_region, -1, SQLITE_STATIC);

    g_ndbprops = 0;
    while (g_ndbprops < MAX_DB_PPROPS && sqlite3_step(stmt) == SQLITE_ROW) {
        db_pprop_t *p = &g_dbprops[g_ndbprops];
        const unsigned char *sig = sqlite3_column_text(stmt, 0);
        const unsigned char *key = sqlite3_column_text(stmt, 1);
        const unsigned char *val = sqlite3_column_text(stmt, 2);

        strncpy(p->signal, sig ? (const char *)sig : "", sizeof(p->signal) - 1);
        strncpy(p->key,    key ? (const char *)key : "", sizeof(p->key) - 1);
        strncpy(p->expect, val ? (const char *)val : "", sizeof(p->expect) - 1);
        p->signal[sizeof(p->signal) - 1] = '\0';
        p->key[sizeof(p->key) - 1]       = '\0';
        p->expect[sizeof(p->expect) - 1] = '\0';
        p->priority = sqlite3_column_int(stmt, 3);
        p->cur[0] = '\0';

        g_ndbprops++;
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    burnedlog("INFO", "loaded %d partner signals for region=%s", g_ndbprops, g_region);
}

static int db_pprop_fires(const db_pprop_t *p)
{
    if (!p->key[0] || !p->expect[0]) return 0;
    if (!strcmp(p->cur, "(unset)") || !strcmp(p->cur, "(err)") || !p->cur[0])
        return 0;

    if (!strchr(p->expect, ',')) {
        return strstr(p->cur, p->expect) != NULL;
    }

    char tmp[160];
    strncpy(tmp, p->expect, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    for (char *tok = strtok(tmp, ","); tok; tok = strtok(NULL, ",")) {
        if (strstr(p->cur, tok)) return 1;
    }
    return 0;
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

/* ── Pass 2: privacy prop table (device-identity + policy, region-agnostic) ──
 *
 * Region-specific partner-ID rows formerly here (Facebook partner ID x2,
 * AppsFlyer preinstall, Spotify partner, Netflix channel, Google client
 * base) have moved to xiaomi_partners.db / g_dbprops (Pass 3) since those
 * values vary by device/region and don't belong hardcoded in source.
 * What remains here is device identity exposure and HyperOS policy
 * behavior that applies regardless of region.
 *
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
static const int g_npprops = 10;

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

/* JSON string escaping (same class of bug fixed via json_escape() in
 * fugitoidd.c and metalheadd.c this arc). */
static void json_escape(const char *in, char *out, size_t out_size) {
    if (!in || out_size == 0) { if (out_size) out[0] = '\0'; return; }
    size_t j = 0;
    for (size_t i = 0; in[i] != '\0' && j + 1 < out_size; i++) {
        unsigned char c = (unsigned char)in[i];
        if (c == '"' || c == '\\') {
            if (j + 2 >= out_size) break;
            out[j++] = '\\';
            out[j++] = (char)c;
        } else if (c == '\n') {
            if (j + 2 >= out_size) break;
            out[j++] = '\\'; out[j++] = 'n';
        } else if (c == '\r') {
            if (j + 2 >= out_size) break;
            out[j++] = '\\'; out[j++] = 'r';
        } else if (c == '\t') {
            if (j + 2 >= out_size) break;
            out[j++] = '\\'; out[j++] = 't';
        } else if (c < 0x20) {
            continue;  /* drop other control chars */
        } else {
            out[j++] = (char)c;
        }
    }
    out[j] = '\0';
}

/* ── Results ──────────────────────────────────────────────────────────── */

static void write_results(int n_invasive, int n_privacy, int n_change,
                           const char *inv_list, const char *priv_list,
                           int scan_num)
{
    FILE *f = fopen(RESULTS_FILE, "w");
    if (!f) {
        burnedlog("ERROR", "cannot write %s: %s", RESULTS_FILE, strerror(errno));
        return;
    }
    time_t t = time(NULL);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", localtime(&t));

    char region_esc[64], inv_esc[512], priv_esc[512];
    json_escape(g_region, region_esc, sizeof(region_esc));
    json_escape(inv_list, inv_esc, sizeof(inv_esc));
    json_escape(priv_list, priv_esc, sizeof(priv_esc));

    fprintf(f,
        "{\n"
        "  \"daemon\": \"" DAEMON_NAME "\",\n"
        "  \"timestamp\": \"%s\",\n"
        "  \"scan_number\": %d,\n"
        "  \"region\": \"%s\",\n"
        "  \"invasive_policy_count\": %d,\n"
        "  \"privacy_signal_count\": %d,\n"
        "  \"change_count\": %d,\n"
        "  \"invasive_list\": \"%s\",\n"
        "  \"privacy_list\": \"%s\",\n"
        "  \"binary_props\": [\n",
        ts, scan_num, region_esc, n_invasive, n_privacy, n_change,
        inv_esc, priv_esc);

    for (int i = 0; i < g_nbprops; i++) {
        const bprop_t *p = &g_bprops[i];
        int fired = (p->invasive_if_1 ==  1 && prop_is_true(p->cur)) ||
                    (p->invasive_if_1 == -1 && prop_is_false(p->cur));
        char key_esc[128], label_esc[128], val_esc[512];
        json_escape(p->key, key_esc, sizeof(key_esc));
        json_escape(p->label, label_esc, sizeof(label_esc));
        json_escape(p->cur, val_esc, sizeof(val_esc));
        fprintf(f,
            "    {\"key\":\"%s\",\"label\":\"%s\",\"value\":\"%s\",\"fired\":%s}%s\n",
            key_esc, label_esc, val_esc, fired ? "true" : "false",
            i < g_nbprops - 1 ? "," : "");
    }
    fprintf(f, "  ],\n  \"privacy_props\": [\n");
    for (int i = 0; i < g_npprops; i++) {
        const pprop_t *p = &g_pprops[i];
        int fired = pprop_fires(p);
        char key_esc[128], label_esc[128], val_esc[512];
        json_escape(p->key, key_esc, sizeof(key_esc));
        json_escape(p->label, label_esc, sizeof(label_esc));
        json_escape(p->cur, val_esc, sizeof(val_esc));
        fprintf(f,
            "    {\"key\":\"%s\",\"label\":\"%s\",\"value\":\"%s\",\"fired\":%s}%s\n",
            key_esc, label_esc, val_esc, fired ? "true" : "false",
            i < g_npprops - 1 ? "," : "");
    }
    fprintf(f, "  ],\n  \"partner_props\": [\n");
    for (int i = 0; i < g_ndbprops; i++) {
        db_pprop_t *p = &g_dbprops[i];
        int fired = db_pprop_fires(p);
        char sig_esc[128], key_esc[128], val_esc[512];
        json_escape(p->signal, sig_esc, sizeof(sig_esc));
        json_escape(p->key, key_esc, sizeof(key_esc));
        json_escape(p->cur, val_esc, sizeof(val_esc));
        fprintf(f,
            "    {\"signal\":\"%s\",\"key\":\"%s\",\"value\":\"%s\",\"priority\":%d,\"fired\":%s}%s\n",
            sig_esc, key_esc, val_esc, p->priority,
            fired ? "true" : "false",
            i < g_ndbprops - 1 ? "," : "");
    }
    fprintf(f, "  ]\n}\n");
    fclose(f);

    burnedlog("INFO", "results written: scan=%d region=%s policy=%d privacy=%d",
              scan_num, g_region, n_invasive, n_privacy);
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
    if (!sig || !sig[0] || !dedup_check_add(sig)) return;
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
        int invasive = (p->invasive_if_1 ==  1 && prop_is_true(p->cur)) ||
                       (p->invasive_if_1 == -1 && prop_is_false(p->cur));

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

    /* ── Pass 3: partner signals (region-loaded from xiaomi_partners.db) ── */
    printf("\n[BURNED]  [PARTNER SIGNALS — region=%s, %d loaded]\n", g_region, g_ndbprops);
    printf("[BURNED]  %-32s %-22s %s\n", "Signal", "Value", "Status");
    printf("[BURNED]  %s\n", "────────────────────────────────────────────────────────");

    for (int i = 0; i < g_ndbprops; i++) {
        db_pprop_t *p = &g_dbprops[i];
        getprop_val(p->key, p->cur, sizeof(p->cur));
        int fires = db_pprop_fires(p);

        printf("[BURNED]  %-32s %-22s%s%s\n",
               p->signal, p->cur, fires ? " [!]" : "",
               fires ? "" : "");

        if (fires) {
            n_privacy++;
            emit_signal(p->signal, p->key);
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
                 "region=%s policy=%d privacy=%d policy_list=%.80s privacy_list=%.80s",
                 g_region, n_invasive, n_privacy, inv_list, priv_list);
        gaveld_emit(DAEMON_NAME, "MIUI_POLICY_ACTIVE", (float)(n_invasive + n_privacy), ctx);
        splinterd_emit("miui_policy", ctx);
    }
    if (n_change > 0) {
        char ctx[128];
        snprintf(ctx, sizeof(ctx), "count=%d", n_change);
        gaveld_emit(DAEMON_NAME, "MIUI_PROPERTY_CHANGED", 0.0, ctx);
        splinterd_emit("miui_change", ctx);
    }

    write_results(n_invasive, n_privacy, n_change, inv_list, priv_list, scan_num);
    g_first = 0;
}

/* ── Main ─────────────────────────────────────────────────────────────── */

int main(void)
{
    burnedlog_init();

    if (!is_enabled()) {
        printf("[BURNED] disabled via syndicatectl — exiting\n");
        return 0;
    }

    printf("[BURNED] MIUI/HyperOS Policy & Privacy Guardian: ONLINE\n");
    burnedlog("INFO", "MIUI/HyperOS Policy & Privacy Guardian: ONLINE");

    detect_region();
    load_partner_db();

    int interval  = get_interval();
    int max_scans = get_max_scans();
    int scan_num  = 0;

    printf("[BURNED] interval=%ds  max_scans=%s  region=%s  partner_signals=%d\n",
           interval, max_scans == 0 ? "unlimited" : "limited", g_region, g_ndbprops);

    for (;;) {
        if (!is_enabled()) {
            printf("[BURNED] disabled — stopping\n");
            burnedlog("INFO", "disabled via syndicatectl — stopping");
            break;
        }
        interval  = get_interval();
        max_scans = get_max_scans();

        scan_num++;
        poll_burned(scan_num);

        if (max_scans > 0 && scan_num >= max_scans) {
            printf("[BURNED] reached scan_count=%d — exiting\n", max_scans);
            burnedlog("INFO", "reached scan_count=%d — exiting", max_scans);
            break;
        }
        sleep(interval);
    }

    if (g_burned_log_fp) fclose(g_burned_log_fp);
    return 0;
}
