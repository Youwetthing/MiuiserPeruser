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
 *   12. DOZE_BYPASS_SYNDROME           (Kimi v2)
 *   13. THERMAL_AMNESIA_SEQUENCE       (Kimi v2)
 *   14. BINDER_PROXIMITY_EXPLOIT       (Kimi v2)
 *   15. MEMORY_EXTENSION_TRAP          (Kimi v2)
 *   16. RADIO_SILENCE_FLIP             (Kimi v2)
 *   17. PRIVACY_SIGNAL_DECOY           (Kimi v2)
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
#include <stdbool.h>
#include <limits.h>
#include <sys/socket.h>
#include <sys/un.h>

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
                         "APRIL|overlordd|%s|%s\n", type, payload);
        if (n > 0) write(fd, buf, (size_t)n);
    }
    close(fd);
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
    splinterd_emit("pattern_detected", detail);
}

/* ── JSON helpers ─────────────────────────────────────────────── */
/* Sentinel-based reads (-1.0 / -1) were retired 2026-07-27: they collided
 * with legitimate values (e.g. a genuinely-negative field) and made the
 * old json_get_int() truncate out-of-range doubles into garbage via a bare
 * (int) cast. All three getters below return true only on a successful
 * parse and write the result through an out-parameter, so "missing" can
 * never be silently read as "found, value happens to be X". Every call
 * site in this file was migrated accordingly — see the fleet-wide
 * found/out-param convention used elsewhere for read_blocked-style checks.
 * json_get_str() was already found/out-param-shaped and is unchanged. */
static bool json_get_double(const char *json, const char *key, double *out) {
    if (!json || !key || !out) return false;
    char needle[128];
    snprintf(needle, sizeof(needle), "\"%s\":", key);
    char *p = strstr(json, needle);
    if (!p) return false;
    p += strlen(needle);
    while (*p == ' ') p++;
    char *end;
    double v = strtod(p, &end);
    if (end == p) return false; /* no digits parsed */
    *out = v;
    return true;
}

static bool json_get_int(const char *json, const char *key, int *out) {
    double d;
    if (!json_get_double(json, key, &d)) return false;
    if (isnan(d) || isinf(d) || d > (double)INT_MAX || d < (double)INT_MIN) return false;
    *out = (int)d;
    return true;
}

static bool json_get_bool(const char *json, const char *key, bool *out) {
    if (!json || !key || !out) return false;
    char needle[128];
    snprintf(needle, sizeof(needle), "\"%s\":", key);
    char *p = strstr(json, needle);
    if (!p) return false;
    p += strlen(needle);
    while (*p == ' ') p++;
    if (strncmp(p, "true",  4) == 0) { *out = true;  return true; }
    if (strncmp(p, "false", 5) == 0) { *out = false; return true; }
    return false;
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

/* ── burned.c disclosed-signal lookup ────────────────────────────
 * burned's binary_props/privacy_props arrays hold entries shaped
 * {"key":"<android prop>","label":"...","value":"...","fired":bool}
 * -- no signal-name field, only the raw prop key. Finds the entry
 * for propkey and reads its "fired" bool, scoped to that one object
 * (stops at the next '}') so a match can't bleed into an adjacent
 * array entry. Used to check whether disclosed HyperOS behavior
 * (RAM extension, aggressive process kills, cleaner, powerkeeper)
 * already explains a memory-pressure symptom before treating it as
 * an attack signal. */
static bool burned_prop_fired(const char *json, const char *propkey, bool *out) {
    if (!json || !propkey || !out) return false;
    char needle[128];
    snprintf(needle, sizeof(needle), "\"key\":\"%s\"", propkey);
    char *p = strstr(json, needle);
    if (!p) return false;
    char *obj_end = strchr(p, '}');
    char *fired = strstr(p, "\"fired\":");
    if (!fired || (obj_end && fired > obj_end)) return false;
    fired += strlen("\"fired\":");
    while (*fired == ' ') fired++;
    if (strncmp(fired, "true",  4) == 0) { *out = true;  return true; }
    if (strncmp(fired, "false", 5) == 0) { *out = false; return true; }
    return false;
}

/* ── Update sliding window ────────────────────────────────────── */
/* NOTE: WindowSnap.thermal_score / .trust_score intentionally keep -1 as
 * an internal "no data" marker — that contract with window_trend_falling()/
 * get_trust()/get_thermal() below is unchanged. Only how this function
 * populates the struct from JSON is migrated. */
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
    s->ts = time(NULL);

    int tmp_i; bool tmp_b; double tmp_d;

    s->thermal_score = (leather && json_get_int(leather, "thermal_score", &tmp_i)) ? tmp_i : -1;
    s->trust_score   = (tiger   && json_get_int(tiger,   "trust_score",   &tmp_i)) ? tmp_i : -1;
    s->spike_events  = (nulld   && json_get_int(nulld,   "total_spike_events", &tmp_i)) ? tmp_i : 0;
    s->kernel_drift  = (shred   && json_get_bool(shred,  "detected", &tmp_b)) ? tmp_b : 0;
    s->selinux_ok    = (granite && json_get_bool(granite,"selinux_enforcing", &tmp_b)) ? tmp_b : 1;

    int tcp4 = 0, tcp6 = 0;
    if (rahzerd) {
        json_get_int(rahzerd, "established_tcp4", &tcp4);
        json_get_int(rahzerd, "established_tcp6", &tcp6);
    }
    s->tcp_total = tcp4 + tcp6;

    s->crashes = (fugi && json_get_int(fugi, "crashes", &tmp_i)) ? tmp_i : 0;
    s->ooms    = (fugi && json_get_int(fugi, "oom_events", &tmp_i)) ? tmp_i : 0;
    s->drain   = (bebopd && json_get_double(bebopd, "drain_mah_h", &tmp_d)) ? tmp_d : 0;

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

    int thermal = 0, throttled = 0, zombies = 0, crashes = 0, ooms = 0;
    bool mem_low = false;
    bool thermal_ok   = json_get_int(leather, "thermal_score", &thermal);
    bool throttled_ok = json_get_int(leather, "throttled_cores", &throttled);
    bool zombies_ok   = json_get_int(ratking, "zombies", &zombies);
    bool crashes_ok   = json_get_int(fugi,    "crashes", &crashes);
    bool ooms_ok      = json_get_int(fugi,    "oom_events", &ooms);
    bool mem_low_ok   = json_get_bool(ratking, "memory_low", &mem_low);

    if (thermal_ok && thermal > 0 && thermal < 70 &&
        ((mem_low_ok && mem_low) || (ooms_ok && ooms > 0)) &&
        ((crashes_ok && crashes > 0) || (zombies_ok && zombies > 3))) {
        char detail[512];
        snprintf(detail, sizeof(detail),
            "Thermal %d/100 + %s + %d crashes + %d zombies — "
            "HyperOS thermal-triggered OOM targeting suspected",
            thermal, (mem_low_ok && mem_low) ? "memory_low" : "OOM events", crashes, zombies);
        double conf = 0.6;
        if (throttled_ok && throttled > 4) conf += 0.1;
        if (ooms_ok && ooms > 0)           conf += 0.15;
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

    int spikes = 0, idle_sec = 0, tcp4 = 0, tcp6 = 0;
    bool spikes_ok = json_get_int(nulld, "total_spike_events", &spikes);
    json_get_int(nulld,   "idle_seconds", &idle_sec);
    json_get_int(rahzerd, "established_tcp4", &tcp4);
    json_get_int(rahzerd, "established_tcp6", &tcp6);

    if (spikes_ok && spikes > 2) {
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

    int sensitive = 0, sen_score = 0, tcp4 = 0, tcp6 = 0;
    double drain = 0;
    bool sensitive_ok = json_get_int(metal, "sensitive_active", &sensitive);
    json_get_int(metal,   "sensor_score", &sen_score);
    json_get_int(rahzerd, "established_tcp4", &tcp4);
    json_get_int(rahzerd, "established_tcp6", &tcp6);
    if (bebopd) json_get_double(bebopd, "drain_mah_h", &drain);

    if (sensitive_ok && sensitive > 2 && (tcp4 + tcp6) > 20) {
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

    int trust = 0, drift = 0, g_score = 0;
    bool selinux_a = false, kern_drift = false, root = false;
    bool trust_ok      = json_get_int(tiger,   "trust_score", &trust);
    bool drift_ok      = json_get_int(tiger,   "drift", &drift);
    bool selinux_a_ok  = json_get_bool(tiger,  "selinux_anomaly", &selinux_a);
    bool kern_drift_ok = json_get_bool(shred,  "detected", &kern_drift);
    bool g_score_ok    = json_get_int(granite, "score", &g_score);
    bool root_ok       = json_get_bool(granite,"root_present", &root);
    (void)g_score_ok;

    int count = 0;
    char detail[512] = "";
    if (trust_ok && trust > 0 && trust < 75) { count++;
        strncat(detail, "low_trust; ", sizeof(detail)-strlen(detail)-1); }
    if (drift_ok && abs(drift) > 5)          { count++;
        strncat(detail, "binder_drift; ", sizeof(detail)-strlen(detail)-1); }
    if (selinux_a_ok && selinux_a)           { count++;
        strncat(detail, "selinux_anomaly; ", sizeof(detail)-strlen(detail)-1); }
    if (kern_drift_ok && kern_drift)         { count++;
        strncat(detail, "kernel_drift; ", sizeof(detail)-strlen(detail)-1); }
    if (root_ok && root)                     { count++;
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

    int sigs = 0, spikes = 0, trust = 100;
    bool sigs_ok = json_get_int(burned, "privacy_signal_count", &sigs);
    if (nulld) json_get_int(nulld, "total_spike_events", &spikes);
    if (tiger) json_get_int(tiger, "trust_score", &trust);

    if (sigs_ok && sigs >= 5 && spikes > 0) {
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

    int thermal = 0, throttl = 0, r_throttl = 0;
    double drain = 0;
    bool thermal_ok = json_get_int(leather, "thermal_score", &thermal);
    json_get_int(leather, "throttled_cores", &throttl);
    if (rocky)  json_get_int(rocky, "throttled_cores", &r_throttl);
    if (bebopd) json_get_double(bebopd, "drain_mah_h", &drain);

    /* HAL temps high but no throttling = thermal lie */
    if (thermal_ok && thermal > 0 && thermal < 75 &&
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

    int spikes = 0, idle_sec = 0, priv_sigs = 0;
    double drain = 0;
    bool spikes_ok = json_get_int(nulld, "total_spike_events", &spikes);
    json_get_int(nulld, "idle_seconds", &idle_sec);
    if (burned)  json_get_int(burned, "privacy_signal_count", &priv_sigs);
    if (bebopd)  json_get_double(bebopd, "drain_mah_h", &drain);
    char screen[16] = {0};
    json_get_str(nulld, "screen", screen, sizeof(screen));

    /* Screen off + spikes + drain + ROM beacons = coordinated exfil */
    if (strcmp(screen, "off") == 0 && spikes_ok && spikes > 0 &&
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

    int sensitive = 0, tcp4 = 0, tcp6 = 0, trust = 100;
    bool sensitive_ok = json_get_int(metal, "sensitive_active", &sensitive);
    json_get_int(rahzerd, "established_tcp4", &tcp4);
    json_get_int(rahzerd, "established_tcp6", &tcp6);
    if (tiger) json_get_int(tiger, "trust_score", &trust);
    char screen[16] = {0};
    json_get_str(nulld, "screen", screen, sizeof(screen));

    /* The invisible pattern — each daemon reports "normal" individually */
    if (sensitive_ok && sensitive > 1 && (tcp4 + tcp6) > 10 &&
        strcmp(screen, "off") == 0 && trust >= 80) {
        char detail[512];
        snprintf(detail, sizeof(detail),
            "%d sensitive sensors active + %d TCP connections "
            "while screen is off + trust score %d/100 (appears normal). "
            "INVISIBLE to individual daemons — only visible via correlation. "
            "Possible sensor-based location tracking during sleep.",
            sensitive, tcp4 + tcp6, trust);
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

    int idle_sec = 0, spikes = 0, anrs = 0;
    double drain = 0;
    json_get_int(nulld, "idle_seconds", &idle_sec);
    bool spikes_ok = json_get_int(nulld, "total_spike_events", &spikes);
    bool drain_ok  = json_get_double(bebopd, "drain_mah_h", &drain);
    if (fugi) json_get_int(fugi, "anrs", &anrs);
    char screen[16] = {0};
    json_get_str(nulld, "screen", screen, sizeof(screen));

    /* Services fighting doze — wake locks held during idle */
    if (strcmp(screen, "off") == 0 && idle_sec > 300 &&
        drain_ok && drain > 100 && spikes_ok && spikes > 1) {
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
/* Unaffected by the JSON-getter migration: consumes WindowSnap fields
 * only, which retain their own -1 "no data" convention via update_window(). */
static void detect_priv_escalation(void) {
    if (!g_window_full && g_window_pos < 3) return;
    int count = g_window_full ? WINDOW_SIZE : g_window_pos;

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
    char *burned  = read_daemon("burned");
    if (!ratking || !fugi) {
        free(ratking); free(fugi); free(rocky); free(bebopd); free(burned); return;
    }

    int avail_mb = 0, zombies = 0, ooms = 0, crashes = 0, throttled = 0;
    bool mem_low = false;
    double drain = 0;

    bool avail_ok   = json_get_int(ratking, "avail_mb", &avail_mb);
    bool mem_low_ok = json_get_bool(ratking, "memory_low", &mem_low);
    json_get_int(ratking, "zombies", &zombies);
    json_get_int(fugi, "oom_events", &ooms);
    json_get_int(fugi, "crashes", &crashes);
    if (rocky)  json_get_int(rocky, "throttled_cores", &throttled);
    if (bebopd) json_get_double(bebopd, "drain_mah_h", &drain);

    /* FIXED 2026-07-27: previously `avail_mb < 0 || avail_mb > 400` treated
     * a failed avail_mb read (old sentinel -1) as satisfying "healthy".
     * Missing telemetry is now its own state and can never read as healthy. */
    bool reported_healthy = mem_low_ok && !mem_low &&
                            avail_ok  && avail_mb > 400;
    bool actual_pressure = (ooms > 0 || crashes > 0 || zombies > 3);

    if (reported_healthy && actual_pressure) {
        /* 2026-07-28: check burned for disclosed HyperOS memory-management
         * behavior that would explain the same symptoms innocently, before
         * treating this as an attack signal. */
        bool disclosed = false;
        char explain[160] = "";
        if (burned) {
            bool f;
            if (burned_prop_fired(burned, "persist.sys.memory_extension_enabled", &f) && f) {
                disclosed = true;
                strncat(explain, "ram_extension; ", sizeof(explain)-strlen(explain)-1);
            }
            if (burned_prop_fired(burned, "persist.sys.miui_scout_binder_full_kill_process", &f) && f) {
                disclosed = true;
                strncat(explain, "binder_full_kill; ", sizeof(explain)-strlen(explain)-1);
            }
            if (burned_prop_fired(burned, "persist.sys.cleaner_level", &f) && f) {
                disclosed = true;
                strncat(explain, "cleaner_aggressive; ", sizeof(explain)-strlen(explain)-1);
            }
            if (burned_prop_fired(burned, "persist.sys.powerkeeper", &f) && f) {
                disclosed = true;
                strncat(explain, "powerkeeper; ", sizeof(explain)-strlen(explain)-1);
            }
        }

        double conf = 0.55;
        if (throttled > 4) conf += 0.1;
        if (drain > 400.0) conf += 0.1;
        if (ooms > 1)       conf += 0.1;
        if (disclosed)      conf -= 0.25;

        if (conf >= 0.30) {
            char detail[512];
            if (disclosed)
                snprintf(detail, sizeof(detail),
                    "Reported healthy (avail=%dMB, memory_low=false) but %d OOMs + "
                    "%d crashes + %d zombies say otherwise — partially explained by "
                    "disclosed HyperOS behavior (%s), confidence reduced accordingly",
                    avail_mb, ooms, crashes, zombies, explain);
            else
                snprintf(detail, sizeof(detail),
                    "Reported healthy (avail=%dMB, memory_low=false) but %d OOMs + "
                    "%d crashes + %d zombies say otherwise — memory state may be "
                    "misreported or artificially masked",
                    avail_mb, ooms, crashes, zombies);
            add_pattern("MEMORY_PRESSURE_MANIPULATION", disclosed ? "WARNING" : "HIGH",
                        detail, "T1414", conf);
        }
    }

    free(ratking); free(fugi); free(rocky); free(bebopd); free(burned);
}

/* ── Pattern 12: Doze bypass syndrome (Kimi v2) ──────────────── */
static void detect_doze_bypass(void) {
    char *nulld  = read_daemon("nulld");
    char *bebopd = read_daemon("bebopd");
    char *fugi   = read_daemon("fugitoidd");
    if (!nulld || !bebopd) { free(nulld); free(bebopd); free(fugi); return; }

    int idle_sec = 0, spikes = 0, anrs = 0;
    double drain = 0;
    json_get_int(nulld, "idle_seconds", &idle_sec);
    bool spikes_ok = json_get_int(nulld, "total_spike_events", &spikes);
    bool drain_ok  = json_get_double(bebopd, "drain_mah_h", &drain);
    if (fugi) json_get_int(fugi, "anrs", &anrs);
    char screen[16] = {0};
    json_get_str(nulld, "screen", screen, sizeof(screen));

    if (strcmp(screen, "off") == 0 && idle_sec > 600 &&
        spikes_ok && spikes > 2 && drain_ok && drain > 80) {
        char detail[512];
        snprintf(detail, sizeof(detail),
            "Screen off %ds but %d spikes + %.1f mAh/hr drain + %d ANRs "
            "-- Doze bypassed. PARTIAL_WAKE_LOCK held by system component. "
            "Precursor to IDLE_EXFILTRATION_BEACON.",
            idle_sec, spikes, drain, anrs);
        double conf = 0.70 + (anrs > 0 ? 0.10 : 0.0) + (drain > 200 ? 0.10 : 0.0);
        add_pattern("DOZE_BYPASS_SYNDROME", "WARNING", detail, "T1426", conf);
    }
    free(nulld); free(bebopd); free(fugi);
}

/* ── Pattern 13: Thermal amnesia sequence (Kimi v2) ──────────── */
static void detect_thermal_amnesia(void) {
    char *leather = read_daemon("leatherheadd");
    char *rocky   = read_daemon("rocksteadyd");
    char *shred   = read_daemon("shredderd");
    char *granite = read_daemon("granitord");
    if (!leather) { free(rocky); free(shred); free(granite); return; }

    int thermal = 0, throttled = 0, r_throttl = 0;
    bool kern_drift = false, selinux = false;
    bool thermal_ok    = json_get_int(leather, "thermal_score", &thermal);
    json_get_int(leather, "throttled_cores", &throttled);
    if (rocky)  json_get_int(rocky, "throttled_cores", &r_throttl);
    bool kern_drift_ok = shred   ? json_get_bool(shred,   "detected",          &kern_drift) : false;
    bool selinux_ok    = granite ? json_get_bool(granite, "selinux_enforcing", &selinux)    : false;

    if (thermal_ok && thermal > 0 && thermal < 60 && (throttled > 2 || r_throttl > 2)
        && kern_drift_ok && kern_drift && selinux_ok && !selinux) {
        char detail[512];
        snprintf(detail, sizeof(detail),
            "Thermal %d/100 but %d cores throttled + kernel drift + "
            "SELinux permissive -- thermal HAL interception. "
            "Dynamic deception: history rewritten post-boot.",
            thermal, throttled + r_throttl);
        add_pattern("THERMAL_AMNESIA_SEQUENCE", "CRITICAL", detail, "T1481", 0.80);
    }
    free(leather); free(rocky); free(shred); free(granite);
}

/* ── Pattern 14: Binder proximity exploit (Kimi v2) ──────────── */
static void detect_binder_proximity(void) {
    char *tiger   = read_daemon("tigerclawd");
    char *ratking = read_daemon("ratkingd");
    char *rahzerd = read_daemon("rahzerd");
    char *burned  = read_daemon("burned");
    if (!tiger || !ratking) {
        free(tiger); free(ratking); free(rahzerd); free(burned); return;
    }

    int trust = 0, drift = 0, orphans = 0, tcp4 = 0, tcp6 = 0, priv = 0;
    bool trust_ok   = json_get_int(tiger,   "trust_score", &trust);
    bool drift_ok   = json_get_int(tiger,   "drift", &drift);
    bool orphans_ok = json_get_int(ratking, "orphans", &orphans);
    if (rahzerd) {
        json_get_int(rahzerd, "established_tcp4", &tcp4);
        json_get_int(rahzerd, "established_tcp6", &tcp6);
    }
    if (burned) json_get_int(burned, "privacy_signal_count", &priv);

    if (drift_ok && drift >= 2 && drift <= 6 && trust_ok && trust > 85 &&
        orphans_ok && orphans > 2 && (tcp4 + tcp6) < 5 && priv >= 5) {
        char detail[512];
        snprintf(detail, sizeof(detail),
            "Binder drift %d but trust %d/100 + %d orphans + %d TCP "
            "-- binder injection staging. Orphans (PPID=1) suggest "
            "parent killed post-exploit. Low TCP = pre-exfil.",
            drift, trust, orphans, tcp4 + tcp6);
        add_pattern("BINDER_PROXIMITY_EXPLOIT", "CRITICAL", detail, "T1437", 0.72);
    }
    free(tiger); free(ratking); free(rahzerd); free(burned);
}

/* ── Pattern 15: Memory extension trap (Kimi v2) ─────────────── */
static void detect_memory_extension_trap(void) {
    char *ratking = read_daemon("ratkingd");
    char *fugi    = read_daemon("fugitoidd");
    char *bebopd  = read_daemon("bebopd");
    char *rocky   = read_daemon("rocksteadyd");
    char *burned  = read_daemon("burned");
    if (!ratking || !fugi) {
        free(ratking); free(fugi); free(bebopd); free(rocky); free(burned); return;
    }

    bool mem_low = false;
    int ooms = 0, crashes = 0, throttled = 0;
    double drain = 0;
    bool mem_low_ok = json_get_bool(ratking, "memory_low", &mem_low);
    json_get_int(fugi, "oom_events", &ooms);
    json_get_int(fugi, "crashes", &crashes);
    if (bebopd) json_get_double(bebopd, "drain_mah_h", &drain);
    if (rocky)  json_get_int(rocky, "throttled_cores", &throttled);

    if (mem_low_ok && !mem_low && ooms > 0 && crashes > 0 && drain > 150) {
        /* 2026-07-28: this pattern accuses HyperOS Memory Extension by name.
         * If burned shows that feature (or aggressive kill/cleaner policy)
         * is disclosed and active, the "trap" framing is wrong -- it's
         * confirmed-but-disclosed behavior, not a hidden trap. */
        bool disclosed = false;
        char explain[160] = "";
        if (burned) {
            bool f;
            if (burned_prop_fired(burned, "persist.sys.memory_extension_enabled", &f) && f) {
                disclosed = true;
                strncat(explain, "ram_extension; ", sizeof(explain)-strlen(explain)-1);
            }
            if (burned_prop_fired(burned, "persist.sys.miui_scout_binder_full_kill_process", &f) && f) {
                disclosed = true;
                strncat(explain, "binder_full_kill; ", sizeof(explain)-strlen(explain)-1);
            }
            if (burned_prop_fired(burned, "persist.sys.cleaner_level", &f) && f) {
                disclosed = true;
                strncat(explain, "cleaner_aggressive; ", sizeof(explain)-strlen(explain)-1);
            }
        }

        double conf = 0.68 + (throttled > 2 ? 0.10 : 0.0);
        if (disclosed) conf -= 0.25;

        if (conf >= 0.30) {
            char detail[512];
            if (disclosed)
                snprintf(detail, sizeof(detail),
                    "memory_low=false but %d OOMs + %d crashes + %.1f mAh/hr "
                    "+ %d throttled -- matches HyperOS Memory Extension pattern, but "
                    "disclosed as active in policy (%s); likely expected behavior, "
                    "not a hidden trap",
                    ooms, crashes, drain, throttled, explain);
            else
                snprintf(detail, sizeof(detail),
                    "memory_low=false but %d OOMs + %d crashes + %.1f mAh/hr "
                    "+ %d throttled -- directed kills via HyperOS Memory Extension. "
                    "Targets likely privacy tools or VPN processes.",
                    ooms, crashes, drain, throttled);
            add_pattern("MEMORY_EXTENSION_TRAP", disclosed ? "LOW" : "WARNING",
                        detail, "T1426", conf);
        }
    }
    free(ratking); free(fugi); free(bebopd); free(rocky); free(burned);
}

/* ── Pattern 16: Radio silence flip (Kimi v2) ────────────────── */
static void detect_radio_silence(void) {
    char *rahzerd = read_daemon("rahzerd");
    char *nulld   = read_daemon("nulld");
    char *bebopd  = read_daemon("bebopd");
    if (!rahzerd || !nulld) { free(rahzerd); free(nulld); free(bebopd); return; }

    int tcp4 = 0, tcp6 = 0, spikes = 0, idle_sec = 0;
    double drain = 0;
    json_get_int(rahzerd, "established_tcp4", &tcp4);
    json_get_int(rahzerd, "established_tcp6", &tcp6);
    bool spikes_ok = json_get_int(nulld, "total_spike_events", &spikes);
    json_get_int(nulld, "idle_seconds", &idle_sec);
    if (bebopd) json_get_double(bebopd, "drain_mah_h", &drain);
    char screen[16] = {0};
    json_get_str(nulld, "screen", screen, sizeof(screen));

    if (strcmp(screen, "off") == 0 && idle_sec > 300 &&
        (tcp4 + tcp6) < 3 && spikes_ok && spikes > 2 && drain > 100) {
        char detail[512];
        snprintf(detail, sizeof(detail),
            "Screen off %ds: only %d TCP + %d spikes + %.1f mAh/hr "
            "-- possible QUIC/UDP fallback. Device may have detected "
            "network monitor and switched to UDP to evade TCP inspection.",
            idle_sec, tcp4 + tcp6, spikes, drain);
        add_pattern("RADIO_SILENCE_FLIP", "WARNING", detail, "T1437", 0.65);
    }
    free(rahzerd); free(nulld); free(bebopd);
}

/* ── Pattern 17: Privacy signal decoy (Kimi v2) ──────────────── */
static void detect_privacy_decoy(void) {
    char *burned  = read_daemon("burned");
    char *tiger   = read_daemon("tigerclawd");
    char *granite = read_daemon("granitord");
    if (!burned) { free(tiger); free(granite); return; }

    int sigs = 0, trust = 100, g_score = 100;
    bool sigs_ok = json_get_int(burned, "privacy_signal_count", &sigs);
    if (tiger)   json_get_int(tiger,   "trust_score", &trust);
    if (granite) json_get_int(granite, "score", &g_score);

    if (sigs_ok && sigs >= 10 && trust > 90 && g_score > 90) {
        char detail[512];
        snprintf(detail, sizeof(detail),
            "%d signals but trust %d/100 + posture %d/100 high "
            "-- decoy flood. High-volume benign signals masking "
            "critical insertion. Review signal list manually.",
            sigs, trust, g_score);
        add_pattern("PRIVACY_SIGNAL_DECOY", "WARNING", detail, "T1429", 0.60);
    }
    free(burned); free(tiger); free(granite);
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
    tlog("INFO", "17 correlation patterns active (5 original + 6 Kimi + 6 more)");

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
        detect_doze_bypass();
        detect_thermal_amnesia();
        detect_binder_proximity();
        detect_memory_extension_trap();
        detect_radio_silence();
        detect_privacy_decoy();

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
