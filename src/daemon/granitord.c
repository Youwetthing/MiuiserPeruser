/*
 * granitord v2.0 — Security Posture & Runtime Integrity Deep Dive
 * CSI Mode: baseline, drift detection, hardware attestation, runtime integrity
 * Upgraded from daytime snapshot to continuous forensic monitoring
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
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <openssl/sha.h>
#include <stdbool.h>


#define DAEMON_NAME     "granitord"
#define VERSION         "2.0"
#define POLL_SEC        30
#define SCORE_WARN      60
#define SCORE_CRITICAL  40
#define BUF_SIZE        1024
#define MAX_BASELINE    256
#define KMSG_BUF        4096

/* ── Baseline state ────────────────────────────────────────────────────────── */
typedef struct {
    char param[64];
    long value;
    time_t first_seen;
} param_baseline_t;

static param_baseline_t baseline_params[MAX_BASELINE];
static int baseline_count = 0;
static int baseline_established = 0;

static char baseline_boot_hash[65] = {0};
static char baseline_vbmeta_hash[65] = {0};

/* Drift confirmation state */
static int last_score = 100;
static int score_drops = 0;
static int confirmed_degradation = 0;

/* ── rish read with dedicated buffer ─────────────────────────────────────────── */
static char *rish_read(const char *cmd, char *buf, size_t bufsize) {
    char full[1024];
    snprintf(full, sizeof(full),
        "RISH_APPLICATION_ID=com.termux "
        "/data/data/com.termux/files/home/Rish/rish -c '%s' 2>/dev/null", cmd);
    FILE *fp = popen(full, "r");
    if (!fp) { buf[0] = 0; return buf; }
    size_t pos = 0;
    char line[512];
    while (fgets(line, sizeof(line), fp) && pos < bufsize - 1) {
        size_t l = strlen(line);
        if (pos + l < bufsize - 1) { memcpy(buf + pos, line, l); pos += l; }
    }
    buf[pos] = 0;
    pclose(fp);
    while (pos > 0 && (buf[pos-1] == '\n' || buf[pos-1] == '\r')) buf[--pos] = 0;
    return buf;
}

static int contains(const char *s, const char *needle) {
    return s && needle && strstr(s, needle) != NULL;
}

/* ── SHA256 helper ──────────────────────────────────────────────────────────── */
static void sha256_file(const char *path, char *out) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    FILE *f = fopen(path, "rb");
    if (!f) { strcpy(out, "UNAVAILABLE"); return; }
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

/* ── Baseline management ──────────────────────────────────────────────────── */
static void establish_baseline(void) {
    fprintf(stderr, "[GRANITOR] Establishing security baseline...\n");
    baseline_count = 0;

    /* Kernel security parameters */
    const char *params[] = {
        "/proc/sys/kernel/kptr_restrict",
        "/proc/sys/kernel/dmesg_restrict",
        "/proc/sys/kernel/perf_event_paranoid",
        "/proc/sys/kernel/randomize_va_space",
        "/proc/sys/fs/protected_hardlinks",
        "/proc/sys/fs/protected_symlinks",
        NULL
    };

    for (int i = 0; params[i] && baseline_count < MAX_BASELINE; i++) {
        FILE *f = fopen(params[i], "r");
        if (f) {
            long v = -1;
            fscanf(f, "%ld", &v);
            fclose(f);
            param_baseline_t *p = &baseline_params[baseline_count];
            strcpy(p->param, params[i]);
            p->value = v;
            p->first_seen = time(NULL);
            baseline_count++;
        }
    }

    /* Boot image hash if accessible */
    char boot_path[256];
    snprintf(boot_path, sizeof(boot_path),
        "find /dev/block -name 'boot*' -o -name 'BOOT*' 2>/dev/null | head -1");
    char buf[512];
    char *boot_dev = rish_read(boot_path, buf, sizeof(buf));
    if (boot_dev && strlen(boot_dev) > 0 && strncmp(boot_dev, "/dev/block", 10) == 0) {
        sha256_file(boot_dev, baseline_boot_hash);
    }

    /* vbmeta hash */
    char vbmeta_path[256];
    snprintf(vbmeta_path, sizeof(vbmeta_path),
        "find /dev/block -name 'vbmeta*' 2>/dev/null | head -1");
    char *vbmeta_dev = rish_read(vbmeta_path, buf, sizeof(buf));
    if (vbmeta_dev && strlen(vbmeta_dev) > 0 && strncmp(vbmeta_dev, "/dev/block", 10) == 0) {
        sha256_file(vbmeta_dev, baseline_vbmeta_hash);
    }

    baseline_established = 1;
    fprintf(stderr, "[GRANITOR] Baseline: %d params, boot=%.12s..., vbmeta=%.12s...\n",
            baseline_count,
            baseline_boot_hash[0] ? baseline_boot_hash : "N/A",
            baseline_vbmeta_hash[0] ? baseline_vbmeta_hash : "N/A");
}

static param_baseline_t* find_baseline_param(const char *path) {
    for (int i = 0; i < baseline_count; i++)
        if (strcmp(baseline_params[i].param, path) == 0)
            return &baseline_params[i];
    return NULL;
}

/* ── Drift detection: parameter changes ─────────────────────────────────────── */
static void detect_param_drift(char *drift_json, size_t drift_size) {
    drift_json[0] = 0;
    for (int i = 0; i < baseline_count; i++) {
        param_baseline_t *b = &baseline_params[i];
        FILE *f = fopen(b->param, "r");
        if (!f) continue;
        long current = -1;
        fscanf(f, "%ld", &current);
        fclose(f);
        if (current != b->value) {
            char entry[512];
            snprintf(entry, sizeof(entry),
                "{\"param\":\"%s\",\"baseline\":%ld,\"current\":%ld,\"changed_at\":\"%ld\"},",
                b->param, b->value, current, (long)time(NULL));
            strncat(drift_json, entry, drift_size - 1);
        }
    }
}

/* ── Hardware attestation check ─────────────────────────────────────────────── */
static void check_hardware_attestation(char *attest_json, size_t attest_size) {
    attest_json[0] = 0;

    /* Check Knox fuse status on Samsung/Xiaomi devices */
    char buf[256];
    char *knox = rish_read("getprop ro.boot.warranty_bit 2>/dev/null", buf, sizeof(buf));
    if (knox && strlen(knox) > 0) {
        int tripped = (strcmp(knox, "1") == 0);
        char entry[256];
        snprintf(entry, sizeof(entry),
            "{\"type\":\"knox_warranty\",\"tripped\":%s,\"raw\":\"%s\"},",
            tripped ? "true" : "false", knox);
        strncat(attest_json, entry, attest_size - 1);
    }

    /* Check verified boot hash consistency */
    char vb_hash[65];
    char vbmeta_path[256];
    snprintf(vbmeta_path, sizeof(vbmeta_path),
        "find /dev/block -name 'vbmeta*' 2>/dev/null | head -1");
    char *vbmeta_dev = rish_read(vbmeta_path, buf, sizeof(buf));
    if (vbmeta_dev && strlen(vbmeta_dev) > 0 && baseline_vbmeta_hash[0]) {
        sha256_file(vbmeta_dev, vb_hash);
        int mismatch = (strcmp(vb_hash, baseline_vbmeta_hash) != 0);
        if (mismatch) {
            char entry[512];
            snprintf(entry, sizeof(entry),
                "{\"type\":\"vbmeta_hash_mismatch\",\"baseline\":\"%.16s...\",\"current\":\"%.16s...\"},",
                baseline_vbmeta_hash, vb_hash);
            strncat(attest_json, entry, attest_size - 1);
        }
    }

    /* Check for TEE/StrongBox availability */
    char *tee = rish_read("ls /dev/tee* 2>/dev/null | head -1", buf, sizeof(buf));
    if (tee && strlen(tee) > 0) {
        char entry[256];
        snprintf(entry, sizeof(entry),
            "{\"type\":\"tee_present\",\"path\":\"%s\"},", tee);
        strncat(attest_json, entry, attest_size - 1);
    }
}

/* ── Runtime integrity: dm-verity & f2fs monitoring ────────────────────────── */
static void check_filesystem_integrity(char *fs_json, size_t fs_size) {
    fs_json[0] = 0;

    /* dm-verity status from dmesg */
    char buf[4096];
    char *verity = rish_read("dmesg 2>/dev/null | grep -i 'dm-verity.*corruption' | tail -3",
                             buf, sizeof(buf));
    if (verity && strlen(verity) > 0) {
        char *line = strtok(verity, "\n");
        while (line) {
            char entry[512];
            snprintf(entry, sizeof(entry),
                "{\"type\":\"verity_corruption\",\"msg\":\"%.200s\"},", line);
            strncat(fs_json, entry, fs_size - 1);
            line = strtok(NULL, "\n");
        }
    }

    /* f2fs errors */
    char *f2fs = rish_read("dmesg 2>/dev/null | grep -iE 'f2fs.*error|f2fs.*corruption' | tail -3",
                           buf, sizeof(buf));
    if (f2fs && strlen(f2fs) > 0) {
        char *line = strtok(f2fs, "\n");
        while (line) {
            char entry[512];
            snprintf(entry, sizeof(entry),
                "{\"type\":\"f2fs_error\",\"msg\":\"%.200s\"},", line);
            strncat(fs_json, entry, fs_size - 1);
            line = strtok(NULL, "\n");
        }
    }

    /* Check if verity is disabled on any partition */
    char *mounts = rish_read("cat /proc/mounts 2>/dev/null | grep -E 'system|vendor' | grep -v 'dm-verity'",
                               buf, sizeof(buf));
    if (mounts && strlen(mounts) > 0) {
        char entry[256];
        snprintf(entry, sizeof(entry),
            "{\"type\":\"verity_disabled\",\"mounts\":\"%.100s\"},", mounts);
        strncat(fs_json, entry, fs_size - 1);
    }
}

/* ── Persistence audit ─────────────────────────────────────────────────────── */
static void audit_persistence(char *persist_json, size_t persist_size) {
    persist_json[0] = 0;

    /* Check for unauthorized init.rc modifications */
    char buf[512];
    char *init_rc = rish_read("find /system/etc/init /vendor/etc/init -name '*.rc' -newer /system/build.prop 2>/dev/null",
                              buf, sizeof(buf));
    if (init_rc && strlen(init_rc) > 0) {
        char entry[256];
        snprintf(entry, sizeof(entry),
            "{\"type\":\"init_rc_modified\",\"files\":\"%.100s\"},", init_rc);
        strncat(persist_json, entry, persist_size - 1);
    }

    /* Check for post-fs-data hooks */
    char *postfs = rish_read("ls /data/adb/post-fs-data.d/ 2>/dev/null",
                              buf, sizeof(buf));
    if (postfs && strlen(postfs) > 0) {
        char entry[256];
        snprintf(entry, sizeof(entry),
            "{\"type\":\"postfs_hooks\",\"files\":\"%.100s\"},", postfs);
        strncat(persist_json, entry, persist_size - 1);
    }

    /* Check for boot image modification */
    if (baseline_boot_hash[0]) {
        char boot_path[256];
        snprintf(boot_path, sizeof(boot_path),
            "find /dev/block -name 'boot*' 2>/dev/null | head -1");
        char *boot_dev = rish_read(boot_path, buf, sizeof(buf));
        if (boot_dev && strlen(boot_dev) > 0) {
            char current_hash[65];
            sha256_file(boot_dev, current_hash);
            if (strcmp(current_hash, baseline_boot_hash) != 0) {
                char entry[512];
                snprintf(entry, sizeof(entry),
                    "{\"type\":\"boot_image_modified\",\"baseline\":\"%.16s...\",\"current\":\"%.16s...\"},",
                    baseline_boot_hash, current_hash);
                strncat(persist_json, entry, persist_size - 1);
            }
        }
    }
}

/* ── Threat correlation with temporal analysis ───────────────────────────── */
static int correlate_threats(
    int score, int enforcing, int rooted,
    const char *drift, const char *attest,
    const char *fs_events, const char *persist,
    char *threat_json, size_t threat_size)
{
    threat_json[0] = 0;
    int confidence = 0;
    char description[256] = "";

    /* Score degradation confirmation */
    if (score < last_score) {
        score_drops++;
        if (score_drops >= 2) {
            confirmed_degradation = 1;
        }
    } else {
        score_drops = 0;
        confirmed_degradation = 0;
    }
    last_score = score;

    /* Correlation rules */
    if (confirmed_degradation && strlen(drift) > 0 && rooted) {
        confidence = 95;
        strcpy(description, "Confirmed degradation + parameter drift + root = active tampering");
    } else if (strlen(fs_events) > 0 && !enforcing) {
        confidence = 90;
        strcpy(description, "Filesystem integrity failure + SELinux permissive = exploit aftermath");
    } else if (strlen(persist) > 0 && rooted) {
        confidence = 85;
        strcpy(description, "Persistence mechanisms + root = maintained access");
    } else if (strlen(attest) > 0 && !rooted) {
        confidence = 80;
        strcpy(description, "Hardware attestation failure without root = possible hardware attack");
    } else if (confirmed_degradation && strlen(drift) > 0) {
        confidence = 75;
        strcpy(description, "Confirmed degradation with parameter drift = policy change or attack");
    }

    if (confidence > 0) {
        snprintf(threat_json, threat_size,
            "{\"type\":\"correlated\",\"confidence\":%d,\"score\":%d,\"description\":\"%s\",\"degradation_confirmed\":%s}",
            confidence, score, description,
            confirmed_degradation ? "true" : "false");
    }
    return confidence;
}

/* ── Strip trailing comma ──────────────────────────────────────────────────── */
static void strip_trailing_comma(char *s) {
    size_t len = strlen(s);
    if (len > 0 && s[len - 1] == ',') s[len - 1] = 0;
}

/* ── JSON output ───────────────────────────────────────────────────────────── */
static void write_json(
    const char *ts, int score, const char *grade,
    const char *selinux, const char *vb_state, const char *vb_mode,
    const char *ro_secure, const char *ro_debug, const char *ro_encrypt,
    int has_su, int has_magisk, int rooted,
    long kptr, long dmesg, long perf, long aslr, long hardlinks, long symlinks,
    const char *drift, const char *attest,
    const char *fs_events, const char *persist,
    const char *threat_indicator)
{
    char d[4096], a[4096], f[4096], p[4096];
    strncpy(d, drift, sizeof(d) - 1); d[sizeof(d) - 1] = 0;
    strncpy(a, attest, sizeof(a) - 1); a[sizeof(a) - 1] = 0;
    strncpy(f, fs_events, sizeof(f) - 1); f[sizeof(f) - 1] = 0;
    strncpy(p, persist, sizeof(p) - 1); p[sizeof(p) - 1] = 0;
    strip_trailing_comma(d);
    strip_trailing_comma(a);
    strip_trailing_comma(f);
    strip_trailing_comma(p);

    FILE *out = fopen("/data/data/com.termux/files/home/MiuiserPeruser/Registry/daemon_results/granitord.json", "w");
    if (!out) {
        fprintf(stderr, "[GRANITOR] ERROR: cannot write JSON\n");
        return;
    }

    fprintf(out,
        "{\n"
        "  \"daemon\": \"granitord\",\n"
        "  \"version\": \"%s\",\n"
        "  \"timestamp\": \"%s\",\n"
        "  \"poll_interval_sec\": %d,\n\n"
        "  \"posture\": {\n"
        "    \"score\": %d,\n"
        "    \"grade\": \"%s\",\n"
        "    \"selinux\": \"%s\",\n"
        "    \"verified_boot_state\": \"%s\",\n"
        "    \"verified_boot_mode\": \"%s\",\n"
        "    \"ro_secure\": \"%s\",\n"
        "    \"ro_debuggable\": \"%s\",\n"
        "    \"encryption\": \"%s\",\n"
        "    \"root_present\": %s,\n"
        "    \"su_found\": %s,\n"
        "    \"magisk\": %s\n"
        "  },\n\n"
        "  \"kernel_params\": {\n"
        "    \"kptr_restrict\": %ld,\n"
        "    \"dmesg_restrict\": %ld,\n"
        "    \"perf_paranoid\": %ld,\n"
        "    \"aslr\": %ld,\n"
        "    \"protected_hardlinks\": %ld,\n"
        "    \"protected_symlinks\": %ld\n"
        "  },\n\n"
        "  \"drift\": {\n"
        "    \"detected\": %s,\n"
        "    \"confirmed_degradation\": %s,\n"
        "    \"param_changes\": [%s]\n"
        "  },\n\n"
        "  \"hardware_attestation\": [%s],\n\n"
        "  \"filesystem_integrity\": [%s],\n\n"
        "  \"persistence_audit\": [%s],\n\n"
        "  \"threat_indicators\": [%s]\n"
        "}\n",
        VERSION, ts, POLL_SEC,
        score, grade, selinux, vb_state, vb_mode,
        ro_secure, ro_debug, ro_encrypt,
        rooted ? "true" : "false",
        has_su ? "true" : "false",
        has_magisk ? "true" : "false",
        kptr, dmesg, perf, aslr, hardlinks, symlinks,
        strlen(d) > 0 ? "true" : "false",
        confirmed_degradation ? "true" : "false",
        d, a, f, p, threat_indicator);

    fflush(out);
    fclose(out);
}

/* ── Main poll ─────────────────────────────────────────────────────────────── */
static void poll_security(void) {
    int score = 100;
    char buf[4096];

    char ts[32];
    time_t t = time(NULL);
    strftime(ts, sizeof(ts), "%H:%M:%S", localtime(&t));

    printf("\n[GRANITOR] ── Security Posture %s ──\n", ts);

    /* ── SELinux with fallback ─────────────────────────────────────────── */
    char selinux[32] = "unknown";
    char *se = rish_read("getenforce 2>/dev/null", buf, sizeof(buf));
    if (se && strlen(se) > 0 && !contains(se, "Permission")) {
        strncpy(selinux, se, sizeof(selinux) - 1);
        selinux[sizeof(selinux) - 1] = 0;
    } else {
        char *prop = rish_read("getprop ro.boot.selinux 2>/dev/null", buf, sizeof(buf));
        if (prop && strlen(prop) > 0) {
            strncpy(selinux, prop, sizeof(selinux) - 1);
            selinux[sizeof(selinux) - 1] = 0;
        }
    }
    int enforcing = contains(selinux, "enforcing") || contains(selinux, "Enforcing");
    if (!enforcing) score -= 25;
    printf("[GRANITOR]  SELinux: %-12s %s\n", selinux, enforcing ? "ok" : "!!! PERMISSIVE");

    /* ── Verified Boot ─────────────────────────────────────────────────── */
    char vb_state[32], vb_mode[32], flash_lock[16];
    char *vbs = rish_read("getprop ro.boot.verifiedbootstate", buf, sizeof(buf));
    strncpy(vb_state, vbs && strlen(vbs) > 0 ? vbs : "unknown", sizeof(vb_state) - 1);
    vb_state[sizeof(vb_state) - 1] = 0;
    char *vbm = rish_read("getprop ro.boot.veritymode", buf, sizeof(buf));
    strncpy(vb_mode, vbm && strlen(vbm) > 0 ? vbm : "unknown", sizeof(vb_mode) - 1);
    vb_mode[sizeof(vb_mode) - 1] = 0;
    char *fl = rish_read("getprop ro.boot.flash.locked", buf, sizeof(buf));
    strncpy(flash_lock, fl && strlen(fl) > 0 ? fl : "unknown", sizeof(flash_lock) - 1);
    flash_lock[sizeof(flash_lock) - 1] = 0;

    int vb_ok = (strcmp(vb_state, "green") == 0);
    int fl_ok = (strcmp(flash_lock, "1") == 0);
    if (!vb_ok) score -= 15;
    if (!fl_ok) score -= 10;
    printf("[GRANITOR]  Verified Boot: %s mode=%s flash=%s %s\n",
           vb_state, vb_mode, flash_lock, (vb_ok && fl_ok) ? "ok" : "WARNING");

    /* ── ro.secure / ro.debuggable ─────────────────────────────────────── */
    char ro_secure[8], ro_debug[8], ro_encrypt[32];
    char *rs = rish_read("getprop ro.secure", buf, sizeof(buf));
    strncpy(ro_secure, rs && strlen(rs) > 0 ? rs : "(unset)", sizeof(ro_secure) - 1);
    ro_secure[sizeof(ro_secure) - 1] = 0;
    char *rd = rish_read("getprop ro.debuggable", buf, sizeof(buf));
    strncpy(ro_debug, rd && strlen(rd) > 0 ? rd : "(unset)", sizeof(ro_debug) - 1);
    ro_debug[sizeof(ro_debug) - 1] = 0;
    char *re = rish_read("getprop ro.crypto.state", buf, sizeof(buf));
    strncpy(ro_encrypt, re && strlen(re) > 0 ? re : "(unset)", sizeof(ro_encrypt) - 1);
    ro_encrypt[sizeof(ro_encrypt) - 1] = 0;

    int secure_ok = (strcmp(ro_secure, "1") == 0);
    int debug_bad = (strcmp(ro_debug, "1") == 0);
    if (!secure_ok) score -= 10;
    if (debug_bad) score -= 10;
    printf("[GRANITOR]  ro.secure=%s ro.debuggable=%s encryption=%s %s\n",
           ro_secure, ro_debug, ro_encrypt,
           (secure_ok && !debug_bad) ? "ok" : "WARNING");

    /* ── Root indicators ─────────────────────────────────────────────── */
    int has_su = access("/system/bin/su", F_OK) == 0 ||
                 access("/system/xbin/su", F_OK) == 0 ||
                 access("/sbin/su", F_OK) == 0 ||
                 access("/su/bin/su", F_OK) == 0;
    int has_magisk = access("/data/adb/magisk", F_OK) == 0 ||
                     access("/data/adb/modules", F_OK) == 0;
    int rooted = has_su || has_magisk;
    if (rooted) score -= 20;
    printf("[GRANITOR]  Root: su=%s magisk=%s %s\n",
           has_su ? "FOUND" : "none",
           has_magisk ? "FOUND" : "none",
           rooted ? "!!! ROOTED" : "ok");

    /* ── Kernel params ───────────────────────────────────────────────── */
    long kptr = -1, dmesg = -1, perf = -1, aslr = -1, hardlinks = -1, symlinks = -1;
    FILE *f = fopen("/proc/sys/kernel/kptr_restrict", "r");
    if (f) { fscanf(f, "%ld", &kptr); fclose(f); }
    f = fopen("/proc/sys/kernel/dmesg_restrict", "r");
    if (f) { fscanf(f, "%ld", &dmesg); fclose(f); }
    f = fopen("/proc/sys/kernel/perf_event_paranoid", "r");
    if (f) { fscanf(f, "%ld", &perf); fclose(f); }
    f = fopen("/proc/sys/kernel/randomize_va_space", "r");
    if (f) { fscanf(f, "%ld", &aslr); fclose(f); }
    f = fopen("/proc/sys/fs/protected_hardlinks", "r");
    if (f) { fscanf(f, "%ld", &hardlinks); fclose(f); }
    f = fopen("/proc/sys/fs/protected_symlinks", "r");
    if (f) { fscanf(f, "%ld", &symlinks); fclose(f); }

    if (kptr < 1) score -= 2;
    if (dmesg < 1) score -= 2;
    if (aslr < 2) score -= 3;
    if (!hardlinks) score -= 1;
    if (!symlinks) score -= 1;

    printf("[GRANITOR]  Kernel: kptr=%ld dmesg=%ld perf=%ld aslr=%ld hardlinks=%ld symlinks=%ld\n",
           kptr, dmesg, perf, aslr, hardlinks, symlinks);

    /* ── Deep dive: drift detection ──────────────────────────────────── */
    char drift_json[4096] = "";
    if (baseline_established) {
        detect_param_drift(drift_json, sizeof(drift_json));
    }

    /* ── Deep dive: hardware attestation ───────────────────────────────── */
    char attest_json[4096] = "";
    check_hardware_attestation(attest_json, sizeof(attest_json));

    /* ── Deep dive: filesystem integrity ──────────────────────────────── */
    char fs_json[4096] = "";
    check_filesystem_integrity(fs_json, sizeof(fs_json));

    /* ── Deep dive: persistence audit ─────────────────────────────────── */
    char persist_json[4096] = "";
    audit_persistence(persist_json, sizeof(persist_json));

    /* ── Threat correlation ──────────────────────────────────────────── */
    char threat_json[512] = "";
    int confidence = correlate_threats(score, enforcing, rooted,
                                       drift_json, attest_json,
                                       fs_json, persist_json,
                                       threat_json, sizeof(threat_json));

    if (score < 0) score = 0;
    const char *grade = score >= 90 ? "SECURE"
                      : score >= 70 ? "CAUTION"
                      : score >= 50 ? "AT RISK"
                      : "COMPROMISED";

    printf("[GRANITOR]  ── Score: %d/100 [%s] confidence=%d ──\n", score, grade, confidence);
    if (strlen(threat_json) > 0)
        printf("[GRANITOR]  THREAT: %s\n", threat_json);
    if (strlen(drift_json) > 0)
        printf("[GRANITOR]  DRIFT: %s\n", drift_json);
    if (strlen(attest_json) > 0)
        printf("[GRANITOR]  ATTEST: %s\n", attest_json);
    if (strlen(fs_json) > 0)
        printf("[GRANITOR]  FS: %s\n", fs_json);
    if (strlen(persist_json) > 0)
        printf("[GRANITOR]  PERSIST: %s\n", persist_json);
    fflush(stdout);

    /* Emit to gaveld on critical findings */
    if (score < SCORE_CRITICAL || confidence >= 90) {
        gaveld_emit(DAEMON_NAME, "SECURITY_CRITICAL", (float)confidence / 100.0f, threat_json);
    } else if (score < SCORE_WARN || confidence >= 75) {
        gaveld_emit(DAEMON_NAME, "SECURITY_WARN", (float)confidence / 100.0f, threat_json);
    }

    write_json(ts, score, grade, selinux, vb_state, vb_mode,
               ro_secure, ro_debug, ro_encrypt,
               has_su, has_magisk, rooted,
               kptr, dmesg, perf, aslr, hardlinks, symlinks,
               drift_json, attest_json, fs_json, persist_json, threat_json);
}

/* ── Main ───────────────────────────────────────────────────────────────── */
int main(void) {
    g_running = true;
    bexec_init();
    printf("[GRANITOR] v%s Security Posture Deep Dive: ONLINE\n", VERSION);
    printf("[GRANITOR] Establishing baseline...\n");
    establish_baseline();
    printf("[GRANITOR] Poll interval: %ds | CSI Mode: ACTIVE\n", POLL_SEC);
    fflush(stdout);

    while (g_running) {
        poll_security();
        sleep(POLL_SEC);
    }

    printf("[GRANITOR] Shutdown.\n");
    return 0;
}
