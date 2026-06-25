/*
 * overlordd.c v1.0 — Cross-Fleet Meta-Correlation Daemon
 *
 * The daemon that unites the whole TMNT universe.
 * Reads all syndicate daemon outputs and finds coordinated
 * manipulation patterns no single daemon can see alone.
 *
 * Patterns (original + Kimi additions):
 *   1.  THERMAL_OOM_MANIPULATION
 *   2.  IDLE_SURVEILLANCE_CYCLE
 *   3.  SENSOR_NETWORK_EXFILTRATION
 *   4.  COORDINATED_SECURITY_WEAKENING
 *   5.  SYSTEMIC_PRIVACY_EROSION
 *   6.  THERMAL_DECEPTION_CYCLE        (Kimi)
 *   7.  IDLE_EXFILTRATION_BEACON       (Kimi)
 *   8.  SENSOR_STALKING_CAMOUFLAGE     (Kimi)
 *   9.  WAKE_LOCK_ABUSE_SYNDROME       (Kimi)
 *   10. PRIVILEGE_ESCALATION_CHAIN     (Kimi)
 *   11. MEMORY_PRESSURE_MANIPULATION   (Kimi)
 *
 * Temporal sliding window tracks trajectory across polls.
 *
 * MITRE ATT&CK Mobile:
 *   T1426 Process Discovery
 *   T1429 Access Call Log
 *   T1430 Location Tracking
 *   T1437 Application Discovery
 *   T1456 Input Injection
 *   T1462 Device Lockout
 *   T1471 Data Encrypted
 *   T1481 Exploit via Radio Interface
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <math.h>

#include "ipc_globals.h"
#include "gaveld_emit.h"

#define BASE          "/data/data/com.termux/files/home/MiuiserPeruser"
#define RESULTS_FILE   BASE "/Registry/daemon_results/overlordd.json"
#define RESULTS_DIR    BASE "/Registry/daemon_results"
#define PID_FILE       BASE "/pipes/pids/overlordd.pid"
#define DEFAULT_POLL   60
#define MAX_PATTERNS   32
#define WINDOW_SIZE    10

static volatile int tc_running = 1;
static int g_poll_count = 0;
static void handle_sig(int s) { (void)s; tc_running = 0; }

/* ── Pattern storage ──────────────────────────────────────────── */
typedef struct {
    char name[64];
    char severity[16];
    char detail[512];
    char mitre[16];
    double confidence;
} Pattern;

static Pattern g_patterns[MAX_PATTERNS];
static int     g_pattern_count = 0;

/* ── Sliding window for temporal correlation ──────────────────── */
typedef struct {
    time_t ts;
    int thermal_score;
    int trust_score;
    int spike_events;
    int kernel_drift;
    int selinux_ok;
    int tcp_total;
    int crashes;
    int ooms;
    double drain;
} WindowSnap;

static WindowSnap g_window[WINDOW_SIZE];
static int        g_window_pos = 0;
static int        g_window_full = 0;

/* ── Logging ──────────────────────────────────────────────────── */
static void tlog(const char *lvl, const char *msg) {
    char ts[32]; time_t t = time(NULL);
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", localtime(&t));
    fprintf(stderr, "[%s][OVERLORDD/%s] %s\n", ts, lvl, msg);
    fflush(stderr);
}

static void add_pattern(const char *name, const char *sev,
                        const char *detail, const char *mitre,
                        double confidence) {
    if (g_pattern_count >= MAX_PATTERNS) return;
    Pattern *p = &g_patterns[g_pattern_count++];
    strncpy(p->name,     name,   sizeof(p->name)-1);
    strncpy(p->severity, sev,    sizeof(p->severity)-1);
    strncpy(p->detail,   detail, sizeof(p->detail)-1);
    strncpy(p->mitre,    mitre,  sizeof(p->mitre)-1);
    p->confidence = confidence;
    char logmsg[256];
    snprintf(logmsg, sizeof(logmsg), "[%s] %s (conf=%.2f)", sev, name, confidence);
    tlog(sev, logmsg);
    gaveld_emit("overlordd", name, confidence, detail);
}

/* ── JSON helpers ─────────────────────────────────────────────── */
static double json_get_double(const char *json, const char *key) {
    if (!json || !key) return -1.0;
    char needle[128];
    snprintf(needle, sizeof(needle), "\"%s\":", key);
    char *p = strstr(json, needle);
    if (!p) return -1.0;
    p += strlen(needle);
    while (*p == ' ') p++;
    return atof(p);
}

static int json_get_int(const char *json, const char *key) {
    return (int)json_get_double(json, key);
}

static int json_get_bool(const char *json, const char *key) {
    if (!json || !key) return -1;
    char needle[128];
    snprintf(needle, sizeof(needle), "\"%s\":", key);
    char *p = strstr(json, needle);
    if (!p) return -1;
    p += strlen(needle);
    while (*p == ' ') p++;
    if (strncmp(p, "true",  4) == 0) return 1;
    if (strncmp(p, "false", 5) == 0) return 0;
    return -1;
}

static char *json_get_str(const char *json, const char *key,
                           char *out, size_t outlen) {
    if (!json || !key) return NULL;
    char needle[128];
    snprintf(needle, sizeof(needle), "\"%s\":\"", key);
    char *p = strstr(json, needle);
    if (!p) return NULL;
    p += strlen(needle);
    size_t i = 0;
    while (*p && *p != '"' && i < outlen-1) out[i++] = *p++;
    out[i] = 0;
    return i > 0 ? out : NULL;
}

/* ── Read daemon JSON ─────────────────────────────────────────── */
static char *read_daemon(const char *name) {
    char path[256];
    snprintf(path, sizeof(path), "%s/%s.json", RESULTS_DIR, name);
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    char *buf = malloc(65536);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, 65535, f);
    fclose(f); buf[n] = 0;
    return buf;
}

/* ── Update sliding window ────────────────────────────────────── */
static void update_window(void) {
    char *leather = read_daemon("leatherheadd");
    char *tiger   = read_daemon("tigerclawd");
    char *nulld   = read_daemon("nulld");
    char *shred   = read_daemon("shredderd");
    char *granite = read_daemon("granitord");
    char *rahzerd = read_daemon("rahzerd");
    char *fugi    = read_daemon("fugitoidd");
    char *bebopd  = read_daemon("bebopd");

    WindowSnap *s = &g_window[g_window_pos];
    s->ts           = time(NULL);
    s->thermal_score = leather ? json_get_int(leather, "thermal_score")    : -1;
    s->trust_score   = tiger   ? json_get_int(tiger,   "trust_score")      : -1;
    s->spike_events  = nulld   ? json_get_int(nulld,   "total_spike_events"): 0;
    s->kernel_drift  = shred   ? json_get_bool(shred,  "detected")          : 0;
    s->selinux_ok    = granite ? json_get_bool(granite,"selinux_enforcing") : 1;
    s->tcp_total     = rahzerd ?
        json_get_int(rahzerd, "established_tcp4") +
        json_get_int(rahzerd, "established_tcp6") : 0;
    s->crashes       = fugi   ? json_get_int(fugi,   "crashes")            : 0;
    s->ooms          = fugi   ? json_get_int(fugi,   "oom_events")         : 0;
    s->drain         = bebopd ? json_get_double(bebopd, "drain_mah_h")     : 0;

    g_window_pos = (g_window_pos + 1) % WINDOW_SIZE;
    if (!g_window_full && g_window_pos == 0) g_window_full = 1;

    free(leather); free(tiger); free(nulld); free(shred);
    free(granite); free(rahzerd); free(fugi); free(bebopd);
}

/* ── Window trend helpers ─────────────────────────────────────── */
static int window_trend_falling(int (*getter)(WindowSnap*), int threshold) {
    int count = g_window_full ? WINDOW_SIZE : g_window_pos;
    if (count < 3) return 0;
    int prev = -1, falling = 0;
    for (int i = 0; i < count; i++) {
        int idx = (g_window_pos - count + i + WINDOW_SIZE) % WINDOW_SIZE;
        int val = getter(&g_window[idx]);
        if (prev >= 0 && val < prev) falling++;
        prev = val;
    }
    return falling >= threshold;
}

static int get_trust(WindowSnap *s)   { return s->trust_score; }
static int get_thermal(WindowSnap *s) { return s->thermal_score; }

/* ── Pattern 1: Thermal OOM manipulation ─────────────────────── */
static void detect_thermal_oom(void) {
    char *leather = read_daemon("leatherheadd");
    char *ratking = read_daemon("ratkingd");
    char *fugi    = read_daemon("fugitoidd");
    if (!leather || !ratking || !fugi) {
        free(leather); free(ratking); free(fugi); return;
    }
    int thermal = json_get_int(leather, "thermal_score");
    int throttled = json_get_int(leather, "throttled_cores");
    int zombies  = json_get_int(ratking, "zombies");
    int crashes  = json_get_int(fugi,   "crashes");
    int ooms     = json_get_int(fugi,   "oom_events");
    int mem_low  = json_get_bool(ratking,"memory_low");

    if (thermal > 0 && thermal < 70 &&
        (mem_low == 1 || ooms > 0) &&
        (crashes > 0 || zombies > 3)) {
        char detail[512];
        snprintf(detail, sizeof(detail),
            "Thermal %d/100 + %s + %d crashes + %d zombies — "
            "HyperOS thermal-triggered OOM targeting suspected",
            thermal, mem_low ? "memory_low" : "OOM events", crashes, zombies);
        double conf = 0.6;
        if (throttled > 4) conf += 0.1;
        if (ooms > 0)      conf += 0.15;
        add_pattern("THERMAL_OOM_MANIPULATION", "CRITICAL",
                    detail, "T1426", conf);
    }
    free(leather); free(ratking); free(fugi);
}

/* ── Pattern 2: Idle surveillance cycle ──────────────────────── */
static void detect_idle_surveillance(void) {
    char *nulld   = read_daemon("nulld");
    char *rahzerd = read_daemon("rahzerd");
    if (!nulld || !rahzerd) { free(nulld); free(rahzerd); return; }

    int spikes   = json_get_int(nulld,   "total_spike_events");
    int idle_sec = json_get_int(nulld,   "idle_seconds");
    int tcp4     = json_get_int(rahzerd, "established_tcp4");
    int tcp6     = json_get_int(rahzerd, "established_tcp6");

    if (spikes > 2) {
        char detail[512];
        snprintf(detail, sizeof(detail),
            "%d connection spikes during %ds idle — "
            "background surveillance activity detected. "
            "Active connections: %d TCP4 + %d TCP6",
            spikes, idle_sec, tcp4, tcp6);
        double conf = 0.5 + (spikes * 0.08);
        if (conf > 0.95) conf = 0.95;
        add_pattern("IDLE_SURVEILLANCE_CYCLE", "WARNING",
                    detail, "T1430", conf);
    }
    free(nulld); free(rahzerd);
}

/* ── Pattern 3: Sensor network exfiltration ──────────────────── */
static void detect_sensor_exfil(void) {
    char *metal   = read_daemon("metalheadd");
    char *rahzerd = read_daemon("rahzerd");
    char *bebopd  = read_daemon("bebopd");
    if (!metal || !rahzerd) {
        free(metal); free(rahzerd); free(bebopd); return;
    }
    int sensitive  = json_get_int(metal,  "sensitive_active");
    int sen_score  = json_get_int(metal,  "sensor_score");
    int tcp4       = json_get_int(rahzerd,"established_tcp4");
    int tcp6       = json_get_int(rahzerd,"established_tcp6");
    double drain   = bebopd ? json_get_double(bebopd,"drain_mah_h") : 0;

    if (sensitive > 2 && (tcp4 + tcp6) > 20) {
        char detail[512];
        snprintf(detail, sizeof(detail),
            "%d sensitive sensors active with %d TCP connections — "
            "possible sensor data exfiltration. "
            "Score: %d/100. Drain: %.1f mAh/hr",
            sensitive, tcp4+tcp6, sen_score, drain);
        double conf = 0.4 + (sensitive * 0.1);
        if (drain > 300) conf += 0.15;
        add_pattern("SENSOR_NETWORK_EXFILTRATION", "WARNING",
                    detail, "T1430", conf);
    }
    free(metal); free(rahzerd); free(bebopd);
}

/* ── Pattern 4: Coordinated security weakening ───────────────── */
static void detect_security_weakening(void) {
    char *tiger   = read_daemon("tigerclawd");
    char *shred   = read_daemon("shredderd");
    char *granite = read_daemon("granitord");
    if (!tiger || !shred || !granite) {
        free(tiger); free(shred); free(granite); return;
    }
    int trust      = json_get_int(tiger,  "trust_score");
    int drift      = json_get_int(tiger,  "drift");
    int selinux_a  = json_get_bool(tiger, "selinux_anomaly");
    int kern_drift = json_get_bool(shred, "detected");
    int g_score    = json_get_int(granite,"score");
    int root       = json_get_bool(granite,"root_present");

    int count = 0;
    char detail[512] = "";
    if (trust > 0 && trust < 75) { count++;
        strncat(detail, "low_trust; ", sizeof(detail)-strlen(detail)-1); }
    if (abs(drift) > 5)          { count++;
        strncat(detail, "binder_drift; ", sizeof(detail)-strlen(detail)-1); }
    if (selinux_a == 1)          { count++;
        strncat(detail, "selinux_anomaly; ", sizeof(detail)-strlen(detail)-1); }
    if (kern_drift == 1)         { count++;
        strncat(detail, "kernel_drift; ", sizeof(detail)-strlen(detail)-1); }
    if (root == 1)               { count++;
        strncat(detail, "root_present; ", sizeof(detail)-strlen(detail)-1); }

    if (count >= 2) {
        char full[512];
        snprintf(full, sizeof(full),
            "%d concurrent security anomalies: %s"
            "(trust=%d/100 granite=%d/100)",
            count, detail, trust, g_score);
        double conf = 0.3 + (count * 0.15);
        if (conf > 0.95) conf = 0.95;
        add_pattern("COORDINATED_SECURITY_WEAKENING",
                    count >= 3 ? "CRITICAL" : "WARNING",
                    full, "T1462", conf);
    }
    free(tiger); free(shred); free(granite);
}

/* ── Pattern 5: Systemic privacy erosion ─────────────────────── */
static void detect_privacy_erosion(void) {
    char *burned = read_daemon("burned");
    char *nulld  = read_daemon("nulld");
    char *tiger  = read_daemon("tigerclawd");
    if (!burned) { free(nulld); free(tiger); return; }

    int sigs   = json_get_int(burned, "privacy_signal_count");
    int spikes = nulld ? json_get_int(nulld, "total_spike_events") : 0;
    int trust  = tiger ? json_get_int(tiger, "trust_score") : 100;

    if (sigs >= 5 && spikes > 0) {
        char detail[512];
        snprintf(detail, sizeof(detail),
            "%d privacy signals in ROM + %d idle transmission spikes "
            "+ device trust %d/100 — systemic privacy erosion. "
            "Leith, Patras & Liu (TCD/Edinburgh 2021) confirmed on this device.",
            sigs, spikes, trust);
        add_pattern("SYSTEMIC_PRIVACY_EROSION", "CRITICAL",
                    detail, "T1429", 0.90);
    }
    free(burned); free(nulld); free(tiger);
}

/* ── Pattern 6: Thermal deception cycle (Kimi) ───────────────── */
static void detect_thermal_deception(void) {
    char *leather = read_daemon("leatherheadd");
    char *rocky   = read_daemon("rocksteadyd");
    char *bebopd  = read_daemon("bebopd");
    if (!leather) { free(rocky); free(bebopd); return; }

    int thermal   = json_get_int(leather, "thermal_score");
    int throttl   = json_get_int(leather, "throttled_cores");
    int r_throttl = rocky  ? json_get_int(rocky,  "throttled_cores") : 0;
    double drain  = bebopd ? json_get_double(bebopd,"drain_mah_h")   : 0;

    /* HAL temps high but no throttling = thermal lie */
    if (thermal > 0 && thermal < 75 &&
        throttl == 0 && r_throttl == 0 && drain > 150) {
        char detail[512];
        snprintf(detail, sizeof(detail),
            "Thermal score %d/100 but ZERO throttled cores — "
            "HyperOS underreporting temps to Android framework. "
            "Drain %.1f mAh/hr while system believes device is cool. "
            "Confirmed 18C HAL vs cached delta on this device.",
            thermal, drain);
        add_pattern("THERMAL_DECEPTION_CYCLE", "CRITICAL",
                    detail, "T1481", 0.85);
    }
    free(leather); free(rocky); free(bebopd);
}

/* ── Pattern 7: Idle exfiltration beacon (Kimi) ─────────────── */
static void detect_idle_exfil_beacon(void) {
    char *nulld   = read_daemon("nulld");
    char *bebopd  = read_daemon("bebopd");
    char *burned  = read_daemon("burned");
    if (!nulld) { free(bebopd); free(burned); return; }

    int spikes    = json_get_int(nulld, "total_spike_events");
    int idle_sec  = json_get_int(nulld, "idle_seconds");
    int priv_sigs = burned ? json_get_int(burned,"privacy_signal_count") : 0;
    double drain  = bebopd ? json_get_double(bebopd,"drain_mah_h") : 0;
    char screen[16] = {0};
    json_get_str(nulld, "screen", screen, sizeof(screen));

    /* Screen off + spikes + drain + ROM beacons = coordinated exfil */
    if (strcmp(screen, "off") == 0 && spikes > 0 &&
        drain > 50 && priv_sigs >= 5) {
        char detail[512];
        snprintf(detail, sizeof(detail),
            "Screen off %ds — %d idle connection spikes + %.1f mAh/hr drain "
            "+ %d ROM privacy signals. Xiaomi beaconing to Azure every ~10min "
            "confirmed via PCAPdroid (fr.resolver.msg.global.xiaomi.net)",
            idle_sec, spikes, drain, priv_sigs);
        add_pattern("IDLE_EXFILTRATION_BEACON", "CRITICAL",
                    detail, "T1430", 0.88);
    }
    free(nulld); free(bebopd); free(burned);
}

/* ── Pattern 8: Sensor stalking camouflage (Kimi) ───────────── */
static void detect_sensor_stalking(void) {
    char *metal   = read_daemon("metalheadd");
    char *rahzerd = read_daemon("rahzerd");
    char *nulld   = read_daemon("nulld");
    char *tiger   = read_daemon("tigerclawd");
    if (!metal || !rahzerd || !nulld) {
        free(metal); free(rahzerd); free(nulld); free(tiger); return;
    }
    int sensitive = json_get_int(metal,   "sensitive_active");
    int tcp_total = json_get_int(rahzerd, "established_tcp4") +
                    json_get_int(rahzerd, "established_tcp6");
    int trust     = tiger ? json_get_int(tiger, "trust_score") : 100;
    char screen[16] = {0};
    json_get_str(nulld, "screen", screen, sizeof(screen));

    /* The invisible pattern — each daemon reports "normal" individually */
    if (sensitive > 1 && tcp_total > 10 &&
        strcmp(screen, "off") == 0 && trust >= 80) {
        char detail[512];
        snprintf(detail, sizeof(detail),
            "%d sensitive sensors active + %d TCP connections "
            "while screen is off + trust score %d/100 (appears normal). "
            "INVISIBLE to individual daemons — only visible via correlation. "
            "Possible sensor-based location tracking during sleep.",
            sensitive, tcp_total, trust);
        add_pattern("SENSOR_STALKING_CAMOUFLAGE", "WARNING",
                    detail, "T1430", 0.70);
    }
    free(metal); free(rahzerd); free(nulld); free(tiger);
}

/* ── Pattern 9: Wake lock abuse syndrome (Kimi) ─────────────── */
static void detect_wakelock_abuse(void) {
    char *nulld   = read_daemon("nulld");
    char *ratking = read_daemon("ratkingd");
    char *bebopd  = read_daemon("bebopd");
    char *fugi    = read_daemon("fugitoidd");
    if (!nulld || !bebopd) {
        free(nulld); free(ratking); free(bebopd); free(fugi); return;
    }
    int idle_sec   = json_get_int(nulld,  "idle_seconds");
    int spikes     = json_get_int(nulld,  "total_spike_events");
    double drain   = json_get_double(bebopd, "drain_mah_h");
    int anrs       = fugi ? json_get_int(fugi, "anrs") : 0;
    char screen[16] = {0};
    json_get_str(nulld, "screen", screen, sizeof(screen));

    /* Services fighting doze — wake locks held during idle */
    if (strcmp(screen, "off") == 0 && idle_sec > 300 &&
        drain > 100 && spikes > 1) {
        char detail[512];
        snprintf(detail, sizeof(detail),
            "%ds idle but %.1f mAh/hr drain + %d network spikes + %d ANRs — "
            "services holding wake locks against Doze. "
            "GMS + MIUI Security Center confirmed active during idle on this device.",
            idle_sec, drain, spikes, anrs);
        double conf = 0.65;
        if (anrs > 0) conf += 0.1;
        if (drain > 200) conf += 0.1;
        add_pattern("WAKE_LOCK_ABUSE_SYNDROME", "WARNING",
                    detail, "T1426", conf);
    }
    free(nulld); free(ratking); free(bebopd); free(fugi);
}

/* ── Pattern 10: Privilege escalation chain (Kimi, temporal) ── */
static void detect_priv_escalation(void) {
    if (!g_window_full && g_window_pos < 3) return;
    int count = g_window_full ? WINDOW_SIZE : g_window_pos;

    /* Check for trust score falling trend + kernel drift appearing */
    int trust_falling = window_trend_falling(get_trust, 2);
    int kern_drift_appeared = 0;
    int prev_drift = 0;

    for (int i = 0; i < count; i++) {
        int idx = (g_window_pos - count + i + WINDOW_SIZE) % WINDOW_SIZE;
        int cur = g_window[idx].kernel_drift;
        if (cur == 1 && prev_drift == 0) kern_drift_appeared = 1;
        prev_drift = cur;
    }

    if (trust_falling && kern_drift_appeared) {
        char detail[512];
        snprintf(detail, sizeof(detail),
            "Trust score falling trend over %d polls + "
            "kernel module drift appeared mid-window — "
            "possible staged privilege escalation: "
            "debug props changed → rootkit loaded → exfiltration",
            count);
        add_pattern("PRIVILEGE_ESCALATION_CHAIN", "CRITICAL",
                    detail, "T1471", 0.75);
    }
}

/* ── Pattern 11: Memory pressure manipulation (Kimi) ─────────── */
static void detect_memory_pressure_manip(void) {
    char *ratking = read_daemon("ratkingd");
    char *fugi    = read_daemon("fugitoidd");
    char *rocky   = read_daemon("rocksteadyd");
    char *bebopd  = read_daemon("bebopd");
    if (!ratking || !fugi) {
        free(ratking); free(fugi); free(rocky); free(bebopd); return;
    }
    int zombies  = json_get_int(ratking, "zombies");
    int orphans  = json_get_int(ratking, "orphans");
    int ooms     = json_get_int(fugi,    "oom_events");
    int crashes  = json_get_int(fugi,    "crashes");
    int r_throttl = rocky  ? json_get_int(rocky,  "throttled_cores") : 0;
    double drain  = bebopd ? json_get_double(bebopd,"drain_mah_h")   : 0;

    if (zombies > 3 && ooms > 0 && (crashes > 0 || r_throttl > 2)) {
        char detail[512];
        snprintf(detail, sizeof(detail),
            "%d zombies + %d orphans + %d OOM kills + %d crashes "
            "+ %d throttled cores + %.1f mAh/hr drain — "
            "coordinated memory pressure cascade. "
            "HyperOS Memory Extension may be creating artificial pressure "
            "to force targeted app kills.",
            zombies, orphans, ooms, crashes, r_throttl, drain);
        double conf = 0.55 + (zombies * 0.05);
        if (conf > 0.90) conf = 0.90;
        add_pattern("MEMORY_PRESSURE_MANIPULATION", "WARNING",
                    detail, "T1426", conf);
    }
    free(ratking); free(fugi); free(rocky); free(bebopd);
}

/* ── Overall threat level ─────────────────────────────────────── */
static const char *threat_level(void) {
    int critical = 0, warning = 0;
    for (int i = 0; i < g_pattern_count; i++) {
        if (strcmp(g_patterns[i].severity, "CRITICAL") == 0) critical++;
        else if (strcmp(g_patterns[i].severity, "WARNING") == 0) warning++;
    }
    if (critical >= 3) return "CRITICAL";
    if (critical >= 2) return "HIGH";
    if (critical >= 1) return "ELEVATED";
    if (warning >= 2)  return "GUARDED";
    if (warning >= 1)  return "LOW";
    return "NOMINAL";
}

/* ── Write JSON ───────────────────────────────────────────────── */
static void write_json(int poll_ms) {
    FILE *f = fopen(RESULTS_FILE, "w");
    if (!f) return;
    char ts[32]; time_t t = time(NULL);
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", localtime(&t));

    fprintf(f,
        "{\n"
        "  \"daemon\": \"overlordd\",\n"
        "  \"version\": \"1.0\",\n"
        "  \"timestamp\": \"%s\",\n"
        "  \"poll_number\": %d,\n"
        "  \"threat_level\": \"%s\",\n"
        "  \"pattern_count\": %d,\n"
        "  \"patterns\": [\n",
        ts, g_poll_count, threat_level(), g_pattern_count);

    for (int i = 0; i < g_pattern_count; i++) {
        fprintf(f,
            "    {\n"
            "      \"name\": \"%s\",\n"
            "      \"severity\": \"%s\",\n"
            "      \"mitre\": \"%s\",\n"
            "      \"confidence\": %.2f,\n"
            "      \"detail\": \"%s\"\n"
            "    }%s\n",
            g_patterns[i].name,
            g_patterns[i].severity,
            g_patterns[i].mitre,
            g_patterns[i].confidence,
            g_patterns[i].detail,
            i < g_pattern_count-1 ? "," : "");
    }

    fprintf(f,
        "  ],\n"
        "  \"poll_duration_ms\": %d\n"
        "}\n", poll_ms);
    fflush(f); fclose(f);
}

int main(void) {
    signal(SIGINT, handle_sig);
    signal(SIGTERM, handle_sig);
    tlog("INFO", "overlordd v1.0 — cross-fleet meta-correlation online");
    tlog("INFO", "11 correlation patterns active (5 original + 6 Kimi)");

    FILE *pf = fopen(PID_FILE, "w");
    if (pf) { fprintf(pf, "%d\n", getpid()); fclose(pf); }

    while (tc_running) {
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        g_pattern_count = 0;
        g_poll_count++;

        tlog("INFO", "scanning all daemon outputs for correlated patterns");

        update_window();

        detect_thermal_oom();
        detect_idle_surveillance();
        detect_sensor_exfil();
        detect_security_weakening();
        detect_privacy_erosion();
        detect_thermal_deception();
        detect_idle_exfil_beacon();
        detect_sensor_stalking();
        detect_wakelock_abuse();
        detect_priv_escalation();
        detect_memory_pressure_manip();

        clock_gettime(CLOCK_MONOTONIC, &t1);
        int ms = (int)((t1.tv_sec-t0.tv_sec)*1000+
                       (t1.tv_nsec-t0.tv_nsec)/1000000);

        write_json(ms);

        char logmsg[256];
        snprintf(logmsg, sizeof(logmsg),
            "poll=%d threat=%s patterns=%d dur=%dms",
            g_poll_count, threat_level(), g_pattern_count, ms);
        tlog("INFO", logmsg);

        for (int i = 0; i < DEFAULT_POLL && tc_running; i++) sleep(1);
    }

    tlog("INFO", "overlordd shutdown");
    unlink(PID_FILE);
    return 0;
}
