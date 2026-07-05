/*
 * shredderd v2.1 — Kernel Integrity & Drift Detection Daemon
 * Fixes: rish buffer race, consistent reads, 2-poll drift confirmation,
 *        SELinux fallback, JSON trailing commas, gaveld_emit integration
 */

#include "ipc_globals.h"
#include "backend_exec.h"
#include "gaveld_emit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <openssl/sha.h>
#include <stdbool.h>


#define DAEMON_NAME     "shredderd"
#define VERSION         "2.1"
#define POLL_SEC        30
#define RESULTS_FILE    "/data/data/com.termux/files/home/MiuiserPeruser/Registry/daemon_results/shredderd.json"
#define BASELINE_FILE   "/data/data/com.termux/files/home/MiuiserPeruser/data/shredderd_baseline.json"
#define DRIFT_LOG       "/data/data/com.termux/files/home/MiuiserPeruser/data/shredderd_drift.log"
#define MAX_MODULES     512
#define DRIFT_CONFIRM   2   /* Require 2 consecutive polls with same delta */

/* ── Module baseline entry ────────────────────────────────────────────────── */
typedef struct {
    char name[64];
    char hash_sha256[65];
    size_t size;
    char path[256];
    time_t first_seen;
} mod_baseline_t;

static mod_baseline_t baseline_mods[MAX_MODULES];
static int baseline_mod_count = 0;
static int baseline_established = 0;

/* Drift confirmation state */
static int last_nmod = 0;
static int drift_polls = 0;
static int confirmed_drift = 0;

/* ── Consistent rish read — no static buffer ───────────────────────────────── */
static char *rish_read(const char *cmd, char *buf, size_t bufsize) {
    char full[2048];
    size_t pos = 0;
    char line[512];
    FILE *fp;

    /* Try adb_cli first */
    snprintf(full, sizeof(full),
        "/data/data/com.termux/files/home/.cargo/bin/adb_cli "
        "tcp 127.0.0.1:5555 shell \"%s\" 2>/dev/null", cmd);
    fp = popen(full, "r");
    if (fp) {
        pos = 0;
        while (fgets(line, sizeof(line), fp) && pos < bufsize-1) {
            size_t l = strlen(line);
            if (pos+l < bufsize-1) { memcpy(buf+pos, line, l); pos += l; }
        }
        buf[pos] = 0; pclose(fp);
        while (pos > 0 && (buf[pos-1]=='\n'||buf[pos-1]=='\r')) buf[--pos]=0;
        if (pos > 0) return buf;
    }

    /* Fallback: rish with timeout */
    snprintf(full, sizeof(full),
        "RISH_APPLICATION_ID=com.termux timeout 5 "
        "/data/data/com.termux/files/home/Rish/rish -c '%s' 2>/dev/null", cmd);
    fp = popen(full, "r");
    if (!fp) { buf[0] = 0; return buf; }
    pos = 0;
    while (fgets(line, sizeof(line), fp) && pos < bufsize-1) {
        size_t l = strlen(line);
        if (pos+l < bufsize-1) { memcpy(buf+pos, line, l); pos += l; }
    }
    buf[pos] = 0; pclose(fp);
    while (pos > 0 && (buf[pos-1]=='\n'||buf[pos-1]=='\r')) buf[--pos]=0;
    return buf;
}


static int contains(const char *s, const char *needle) {
    return s && needle && strstr(s, needle) != NULL;
}

/* ── SHA256 helper ───────────────────────────────────────────────────────── */
static void sha256_file(const char *path, char *out) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    FILE *f = fopen(path, "rb");
    if (!f) { strcpy(out, "UNKNOWN"); return; }
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        SHA256_Update(&ctx, buf, n);
    SHA256_Final(hash, &ctx);
    fclose(f);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        sprintf(out + i * 2, "%02x", hash[i]);
    out[64] = 0;
}

/* ── Baseline management — consistent read method ──────────────────────────── */
static void load_baseline(void) {
    FILE *f = fopen(BASELINE_FILE, "r");
    if (!f) return;
    char line[512];
    baseline_mod_count = 0;
    while (fgets(line, sizeof(line), f) && baseline_mod_count < MAX_MODULES) {
        char name[64], hash[65], path[256];
        size_t size;
        time_t ts;
        if (sscanf(line, "%63s %64s %zu %255s %ld", name, hash, &size, path, &ts) == 5) {
            strcpy(baseline_mods[baseline_mod_count].name, name);
            strcpy(baseline_mods[baseline_mod_count].hash_sha256, hash);
            baseline_mods[baseline_mod_count].size = size;
            strcpy(baseline_mods[baseline_mod_count].path, path);
            baseline_mods[baseline_mod_count].first_seen = ts;
            baseline_mod_count++;
        }
    }
    fclose(f);
    baseline_established = (baseline_mod_count > 0);
    fprintf(stderr, "[SHREDDER] Baseline loaded: %d modules\n", baseline_mod_count);
}

static void save_baseline(void) {
    FILE *f = fopen(BASELINE_FILE, "w");
    if (!f) return;
    for (int i = 0; i < baseline_mod_count; i++) {
        fprintf(f, "%s %s %zu %s %ld\n",
            baseline_mods[i].name,
            baseline_mods[i].hash_sha256,
            baseline_mods[i].size,
            baseline_mods[i].path,
            baseline_mods[i].first_seen);
    }
    fclose(f);
}

static void establish_baseline(void) {
    fprintf(stderr, "[SHREDDER] Establishing baseline...\n");
    baseline_mod_count = 0;

    /* Use system() to write modules to a temp file, then read it */
    system("RISH_APPLICATION_ID=com.termux "
           "/data/data/com.termux/files/home/Rish/rish -c "
           "'cat /proc/modules' "
           "> /data/data/com.termux/files/home/MiuiserPeruser/pipes/mods.tmp 2>/dev/null");

    FILE *mp = fopen("/data/data/com.termux/files/home/MiuiserPeruser/pipes/mods.tmp", "r");
    if (!mp) {
        fprintf(stderr, "[SHREDDER] Cannot read modules\n");
        return;
    }
    char mline[256];
    while (fgets(mline, sizeof(mline), mp) && baseline_mod_count < MAX_MODULES) {
        char name[64];
        if (sscanf(mline, "%63s", name) == 1 && strlen(name) > 1) {
            mod_baseline_t *m = &baseline_mods[baseline_mod_count];
            strncpy(m->name, name, 63);
            m->size = 0;
            strcpy(m->path, "UNKNOWN");
            strcpy(m->hash_sha256, "UNKNOWN");
            m->first_seen = time(NULL);
            baseline_mod_count++;
        }
    }
    fclose(mp);
    baseline_established = 1;
    save_baseline();
    fprintf(stderr, "[SHREDDER] Baseline established: %d modules\n", baseline_mod_count);
}

static mod_baseline_t* find_in_baseline(const char *name) {
    for (int i = 0; i < baseline_mod_count; i++)
        if (strcmp(baseline_mods[i].name, name) == 0)
            return &baseline_mods[i];
    return NULL;
}

/* ── Drift detection with 2-poll confirmation ─────────────────────────────── */
static int detect_drift(int current_nmod, char *new_json, size_t new_size,
                        char *removed_json, size_t rem_size) {
    new_json[0] = 0;
    removed_json[0] = 0;

    int delta = current_nmod - baseline_mod_count;

    if (delta == 0) {
        drift_polls = 0;
        confirmed_drift = 0;
        return 0;
    }

    /* Same delta as last poll? */
    if (delta == last_nmod - baseline_mod_count) {
        drift_polls++;
    } else {
        drift_polls = 1;
    }
    last_nmod = current_nmod;

    /* Need 2 consecutive polls with same non-zero delta */
    if (drift_polls < DRIFT_CONFIRM) {
        fprintf(stderr, "[SHREDDER] Drift detected (%d modules) but waiting confirmation (poll %d/%d)\n",
                delta, drift_polls, DRIFT_CONFIRM);
        return 0;
    }

    confirmed_drift = 1;

    /* Build detailed drift report */
    char current_names[MAX_MODULES][64];
    int current_count = 0;
    char buf[8192];
    char *mods = rish_read("cat /proc/modules 2>/dev/null | awk '{print $1}'", buf, sizeof(buf));
    char *line = strtok(mods, "\n");
    while (line && current_count < MAX_MODULES) {
        strncpy(current_names[current_count], line, 63);
        current_names[current_count][63] = 0;
        current_count++;
        line = strtok(NULL, "\n");
    }

    /* New modules */
    for (int i = 0; i < current_count; i++) {
        if (!find_in_baseline(current_names[i])) {
            char entry[512];
            snprintf(entry, sizeof(entry),
                "{\"name\":\"%s\",\"detected_at\":\"%ld\"},",
                current_names[i], (long)time(NULL));
            size_t _nlen = strlen(new_json);
            if (_nlen + strlen(entry) < new_size - 1)
                strncat(new_json, entry, new_size - _nlen - 1);
        }
    }

    /* Removed modules */
    for (int i = 0; i < baseline_mod_count; i++) {
        int found = 0;
        for (int j = 0; j < current_count; j++)
            if (strcmp(baseline_mods[i].name, current_names[j]) == 0) { found = 1; break; }
        if (!found) {
            char entry[512];
            snprintf(entry, sizeof(entry),
                "{\"name\":\"%s\",\"removed_at\":\"%ld\"},",
                baseline_mods[i].name, (long)time(NULL));
            size_t _rlen = strlen(removed_json);
            if (_rlen + strlen(entry) < rem_size - 1)
                strncat(removed_json, entry, rem_size - _rlen - 1);
        }
    }

    return delta;
}

/* ── SELinux with fallback ─────────────────────────────────────────────────── */
static void get_selinux_state(char *out, size_t outsize) {
    char buf[256];
    char *se = rish_read("getenforce 2>/dev/null", buf, sizeof(buf));
    if (se && strlen(se) > 0 && !contains(se, "Permission denied")) {
        strncpy(out, se, outsize - 1);
        out[outsize - 1] = 0;
        return;
    }
    /* Fallback to getprop */
    char *prop = rish_read("getprop ro.boot.selinux 2>/dev/null", buf, sizeof(buf));
    if (prop && strlen(prop) > 0) {
        strncpy(out, prop, outsize - 1);
        out[outsize - 1] = 0;
        return;
    }
    /* Second fallback: check /sys/fs/selinux/enforce directly */
    char *enforce = rish_read("cat /sys/fs/selinux/enforce 2>/dev/null", buf, sizeof(buf));
    if (enforce && strcmp(enforce, "1") == 0) {
        strncpy(out, "enforcing", outsize - 1);
        out[outsize - 1] = 0;
        return;
    } else if (enforce && strcmp(enforce, "0") == 0) {
        strncpy(out, "permissive", outsize - 1);
        out[outsize - 1] = 0;
        return;
    }
    strncpy(out, "unknown", outsize - 1);
    out[outsize - 1] = 0;
}

/* ── Strip trailing comma from JSON array ─────────────────────────────────── */
static void strip_trailing_comma(char *s) {
    size_t len = strlen(s);
    if (len > 0 && s[len - 1] == ',') {
        s[len - 1] = 0;
    }
}

/* ── Threat correlation ───────────────────────────────────────────────────── */
static int correlate_threats(
    int magisk, int kernelsu, int debugfs,
    int drift_confirmed, int drift_delta,
    int root_procs, const char *selinux,
    char *threat_json, size_t threat_size)
{
    threat_json[0] = 0;
    int score = 100;
    int confidence = 0;
    char description[256] = "";

    if (magisk && drift_confirmed && drift_delta > 0 && debugfs) {
        score = 15; confidence = 95;
        strcpy(description, "Persistence + new kernel code + debug interface = likely active rootkit");
    } else if (kernelsu && drift_confirmed && drift_delta != 0) {
        score = 20; confidence = 90;
        strcpy(description, "KernelSU + module drift = kernel tampering detected");
    } else if (drift_confirmed && drift_delta > 0 && !magisk && !kernelsu) {
        score = 60; confidence = 70;
        strcpy(description, "New kernel module without known root manager — investigate");
    } else if (debugfs && root_procs > 5) {
        score = 40; confidence = 80;
        strcpy(description, "debugfs mounted with multiple root processes — attack surface active");
    } else if (!contains(selinux, "enforcing") && !contains(selinux, "Enforcing")) {
        score = 30; confidence = 85;
        strcpy(description, "SELinux not enforcing — MAC bypass possible");
    }

    if (confidence > 0) {
        snprintf(threat_json, threat_size,
            "{\"type\":\"correlated\",\"confidence\":%d,\"score\":%d,\"description\":\"%s\"}",
            confidence, score, description);
    }
    return score;
}

/* ── JSON output — trailing comma safe ────────────────────────────────────── */
static void write_json(
    const char *ts, int score, const char *grade,
    int su_found, int magisk, int kernelsu,
    const char *vb_state, const char *vb_mode,
    int nmod, int debugfs, int root_procs,
    const char *selinux, const char *suspicious_mod,
    const char *new_mods, const char *removed_mods,
    const char *kernel_events, const char *threat_indicator)
{
    /* Strip trailing commas from arrays */
    char new_clean[4096], rem_clean[4096], evt_clean[4096];
    strncpy(new_clean, new_mods, sizeof(new_clean) - 1);
    strncpy(rem_clean, removed_mods, sizeof(rem_clean) - 1);
    strncpy(evt_clean, kernel_events, sizeof(evt_clean) - 1);
    new_clean[sizeof(new_clean) - 1] = 0;
    rem_clean[sizeof(rem_clean) - 1] = 0;
    evt_clean[sizeof(evt_clean) - 1] = 0;
    strip_trailing_comma(new_clean);
    strip_trailing_comma(rem_clean);
    strip_trailing_comma(evt_clean);

    FILE *f = fopen(RESULTS_FILE, "w");
    if (!f) {
        fprintf(stderr, "[SHREDDER] ERROR: cannot write %s\n", RESULTS_FILE);
        return;
    }

    fprintf(f,
        "{\n"
        "  \"daemon\": \"shredderd\",\n"
        "  \"version\": \"%s\",\n"
        "  \"timestamp\": \"%s\",\n"
        "  \"poll_interval_sec\": %d,\n\n"
        "  \"baseline\": {\n"
        "    \"established\": %s,\n"
        "    \"module_count\": %d\n"
        "  },\n\n"
        "  \"integrity\": {\n"
        "    \"score\": %d,\n"
        "    \"grade\": \"%s\",\n"
        "    \"root_present\": %s,\n"
        "    \"su_found\": %d,\n"
        "    \"magisk\": %d,\n"
        "    \"kernelsu\": %d,\n"
        "    \"verified_boot_state\": \"%s\",\n"
        "    \"verified_boot_mode\": \"%s\",\n"
        "    \"selinux\": \"%s\"\n"
        "  },\n\n"
        "  \"drift\": {\n"
        "    \"detected\": %s,\n"
        "    \"confirmed\": %s,\n"
        "    \"delta\": %d,\n"
        "    \"new_modules\": [%s],\n"
        "    \"removed_modules\": [%s]\n"
        "  },\n\n"
        "  \"kernel_events\": [%s],\n\n"
        "  \"threat_indicators\": [%s],\n\n"
        "  \"surface\": {\n"
        "    \"debugfs_mounted\": %s,\n"
        "    \"root_processes\": %d,\n"
        "    \"unknown_modules\": %d,\n"
        "    \"suspicious_module\": \"%s\"\n"
        "  }\n"
        "}\n",
        VERSION, ts, POLL_SEC,
        baseline_established ? "true" : "false", baseline_mod_count,
        score, grade,
        (su_found || magisk || kernelsu) ? "true" : "false",
        su_found, magisk, kernelsu,
        vb_state ? vb_state : "unknown",
        vb_mode ? vb_mode : "unknown",
        selinux,
        (strlen(new_clean) > 0 || strlen(rem_clean) > 0) ? "true" : "false",
        confirmed_drift ? "true" : "false",
        nmod - baseline_mod_count,
        new_clean, rem_clean,
        evt_clean, threat_indicator,
        debugfs ? "true" : "false", root_procs, nmod,
        suspicious_mod ? suspicious_mod : ""
    );

    fflush(f);
    fclose(f);
    fprintf(stderr, "[SHREDDER] JSON written: score=%d grade=%s drift_confirmed=%s\n",
        score, grade, confirmed_drift ? "YES" : "no");
}

/* ── Main poll ───────────────────────────────────────────────────────────── */
static void poll_integrity(void) {
    int score = 100;
    char buf[8192];

    /* su binaries */
    int su_found = 0;
    const char *su_paths[] = {
        "/sbin/su","/system/bin/su","/system/xbin/su",
        "/su/bin/su","/magisk/.core/bin/su", NULL
    };
    for (int i = 0; su_paths[i]; i++) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "test -e %s && echo yes", su_paths[i]);
        char *res = rish_read(cmd, buf, sizeof(buf));
        if (contains(res, "yes")) { su_found = 1; break; }
    }

    /* Magisk / KernelSU */
    char *magisk_res = rish_read("test -d /data/adb/magisk && echo yes", buf, sizeof(buf));
    int magisk = contains(magisk_res, "yes");
    char *ksu_res = rish_read("test -e /sys/kernel/ksu && echo yes", buf, sizeof(buf));
    int kernelsu = contains(ksu_res, "yes");

    /* Verified boot */
    char *vb_state = rish_read("getprop ro.boot.verifiedbootstate", buf, sizeof(buf));
    char *vb_mode  = rish_read("getprop ro.boot.flash.locked", buf, sizeof(buf));
    int vb_ok = contains(vb_state, "green") || contains(vb_state, "yellow");
    if (!vb_ok) score -= 10;

    /* Current modules — same read method as baseline */
    char *mods = rish_read("cat /proc/modules 2>/dev/null | wc -l", buf, sizeof(buf));
    int nmod = mods ? atoi(mods) : 0;

    /* Suspicious module names */
    char suspicious_mod[64] = "";
    char *susp = rish_read("lsmod 2>/dev/null | grep -iE 'frida|hook|inject|rootkit|backdoor' | head -1 | awk '{print $1}'", buf, sizeof(buf));
    if (susp && strlen(susp) > 0) strncpy(suspicious_mod, susp, sizeof(suspicious_mod) - 1);
    if (suspicious_mod[0]) score -= 20;

    /* debugfs */
    char *dfs = rish_read("mount 2>/dev/null | grep -c debugfs", buf, sizeof(buf));
    int debugfs = contains(dfs, "1");
    if (debugfs) score -= 5;

    /* Root processes */
    char *rp = rish_read("ps -A 2>/dev/null | awk '$2==\"root\"' | wc -l", buf, sizeof(buf));
    int root_procs = rp ? atoi(rp) : 0;

    /* SELinux — with fallback chain */
    char selinux[32];
    get_selinux_state(selinux, sizeof(selinux));
    int enforcing = contains(selinux, "enforcing") || contains(selinux, "Enforcing");
    if (!enforcing) score -= 15;

    /* Drift detection with 2-poll confirmation */
    char new_mods[4096] = "", removed_mods[4096] = "";
    int drift_delta = 0;
    if (baseline_established) {
        drift_delta = detect_drift(nmod, new_mods, sizeof(new_mods),
                                   removed_mods, sizeof(removed_mods));
    }

    /* Kernel events — /proc/kmsg blocked by SELinux, use fallback */
    char kernel_events[4096] = "";
    /* Fallback: check dmesg for recent events */
    char *dmesg_recent = rish_read("dmesg 2>/dev/null | tail -20 | grep -E 'module|Oops|avc|verity' | head -5", buf, sizeof(buf));
    if (dmesg_recent && strlen(dmesg_recent) > 0) {
        char *line = strtok(dmesg_recent, "\n");
        while (line) {
            char entry[512];
            snprintf(entry, sizeof(entry), "{\"type\":\"dmesg\",\"msg\":\"%s\"},", line);
            strncat(kernel_events, entry, sizeof(kernel_events) - 1);
            line = strtok(NULL, "\n");
        }
    }

    /* Threat correlation */
    char threat_indicator[512] = "";
    int correlated_score = correlate_threats(magisk, kernelsu, debugfs,
                                              confirmed_drift, drift_delta,
                                              root_procs, selinux,
                                              threat_indicator, sizeof(threat_indicator));
    if (correlated_score < score) score = correlated_score;

    if (score < 0) score = 0;
    const char *grade = score >= 90 ? "INTACT"
                      : score >= 70 ? "DEGRADED"
                      : score >= 50 ? "AT_RISK"
                      : "CRITICAL";

    /* Timestamp */
    time_t t = time(NULL);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", localtime(&t));

    /* Console report */
    printf("[SHREDDER] ── %s ── score=%d [%s] ──\n", ts, score, grade);
    printf("[SHREDDER]  su:%s magisk:%s ksu:%s | vb:%s | selinux:%s\n",
           su_found ? "Y" : "n", magisk ? "Y" : "n", kernelsu ? "Y" : "n",
           vb_state, selinux);
    printf("[SHREDDER]  modules:%d baseline:%d drift:%s confirmed:%s\n",
           nmod, baseline_mod_count,
           (drift_delta != 0) ? "YES" : "no",
           confirmed_drift ? "YES" : "no");
    if (strlen(threat_indicator) > 0)
        printf("[SHREDDER]  THREAT: %s\n", threat_indicator);
    fflush(stdout);

    /* Emit to gaveld if critical */
    if (score < 50 || (confirmed_drift && drift_delta > 0)) {
        gaveld_emit(DAEMON_NAME, "KERNEL_THREAT", 0.0, threat_indicator);
        if (confirmed_drift && drift_delta > 0)
            gaveld_emit(DAEMON_NAME, "NEW_KERNEL_MODULE", 0.0, new_mods);
    }

    write_json(ts, score, grade, su_found, magisk, kernelsu,
               vb_state ? vb_state : "unknown",
               vb_mode ? vb_mode : "unknown",
               nmod, debugfs, root_procs, selinux, suspicious_mod,
               new_mods, removed_mods, kernel_events, threat_indicator);
}

/* ── Main ─────────────────────────────────────────────────────────────────── */
int main(void) {
    g_running = 1;
    bexec_init();
    printf("[SHREDDER] v%s Kernel Integrity Daemon: ONLINE\n", VERSION);
    printf("[SHREDDER] Loading baseline...\n");
    load_baseline();

    if (!baseline_established) {
        printf("[SHREDDER] No baseline found. Establishing now...\n");
        establish_baseline();
    }

    printf("[SHREDDER] Poll interval: %ds | Drift confirm: %d polls | Deep dive: ACTIVE\n",
           POLL_SEC, DRIFT_CONFIRM);
    fflush(stdout);

    while (g_running) {
        poll_integrity();
        sleep(POLL_SEC);
    }

    printf("[SHREDDER] Shutdown complete.\n");
    return 0;
}
