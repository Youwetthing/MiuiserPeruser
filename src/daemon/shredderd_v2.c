/*
 * shredderd v2.0 — Kernel Integrity & Drift Detection Daemon
 * Deep dive: baseline, event-driven monitoring, threat correlation
 * No root-shaming. Reports what it finds.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/inotify.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <openssl/sha.h>

#define DAEMON_NAME     "shredderd"
#define VERSION         "2.0"
#define POLL_SEC        30
#define RESULTS_FILE    "/data/data/com.termux/files/home/MiuiserPeruser/Registry/daemon_results/shredderd.json"
#define BASELINE_FILE   "/data/data/com.termux/files/home/MiuiserPeruser/data/shredderd_baseline.json"
#define KMSG_BUF        8192
#define MAX_MODULES     512
#define MAX_EVENTS      64

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

/* ── rish helper ──────────────────────────────────────────────────────────── */
static char *rish(const char *cmd) {
    static char buf[4096];
    char full[1024];
    snprintf(full, sizeof(full),
        "RISH_APPLICATION_ID=com.termux "
        "/data/data/com.termux/files/home/Rish/rish -c '%s' 2>/dev/null", cmd);
    FILE *fp = popen(full, "r");
    if (!fp) { buf[0] = 0; return buf; }
    size_t pos = 0;
    char line[512];
    while (fgets(line, sizeof(line), fp) && pos < sizeof(buf)-1) {
        size_t l = strlen(line);
        if (pos + l < sizeof(buf)-1) { memcpy(buf+pos, line, l); pos += l; }
    }
    buf[pos] = 0;
    pclose(fp);
    while (pos > 0 && (buf[pos-1]=='\n'||buf[pos-1]=='\r')) buf[--pos]=0;
    return buf;
}

static int contains(const char *s, const char *needle) {
    return s && needle && strstr(s, needle) != NULL;
}

/* ── SHA256 helper ─────────────────────────────────────────────────────────── */
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
        sprintf(out + i*2, "%02x", hash[i]);
    out[64] = 0;
}

static void sha256_string(const char *s, char *out) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)s, strlen(s), hash);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        sprintf(out + i*2, "%02x", hash[i]);
    out[64] = 0;
}

/* ── Baseline management ──────────────────────────────────────────────────── */
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

    char *mods = rish("cat /proc/modules 2>/dev/null | awk '{print $1, $2}'");
    char *line = strtok(mods, "\n");
    while (line && baseline_mod_count < MAX_MODULES) {
        char name[64], size_str[32];
        if (sscanf(line, "%63s %31s", name, size_str) == 2) {
            // Find .ko file
            char find_cmd[256];
            snprintf(find_cmd, sizeof(find_cmd),
                "find /lib/modules /vendor/lib/modules /system/lib/modules -name '%s.ko' 2>/dev/null | head -1",
                name);
            char *path = rish(find_cmd);

            mod_baseline_t *m = &baseline_mods[baseline_mod_count];
            strcpy(m->name, name);
            m->size = atol(size_str);
            strcpy(m->path, path && strlen(path) > 0 ? path : "UNKNOWN");
            if (path && strlen(path) > 0 && strcmp(path, "UNKNOWN") != 0)
                sha256_file(path, m->hash_sha256);
            else
                strcpy(m->hash_sha256, "UNKNOWN");
            m->first_seen = time(NULL);
            baseline_mod_count++;
        }
        line = strtok(NULL, "\n");
    }

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

/* ── Event-driven: monitor /proc/kmsg ─────────────────────────────────────── */
static int kmsg_fd = -1;

static int init_kmsg_monitor(void) {
    kmsg_fd = open("/proc/kmsg", O_RDONLY | O_NONBLOCK);
    if (kmsg_fd < 0) {
        fprintf(stderr, "[SHREDDER] Cannot open /proc/kmsg: %s\n", strerror(errno));
        return -1;
    }
    fprintf(stderr, "[SHREDDER] Kernel message monitor active\n");
    return 0;
}

static void poll_kmsg(char *events_out, size_t events_size) {
    if (kmsg_fd < 0) return;
    char buf[KMSG_BUF];
    ssize_t n = read(kmsg_fd, buf, sizeof(buf)-1);
    if (n > 0) {
        buf[n] = 0;
        // Parse for interesting events
        if (contains(buf, "module loaded")) {
            strncat(events_out, "{\"type\":\"module_load\",\"msg\":\"", events_size-1);
            strncat(events_out, buf, events_size-1);
            strncat(events_out, "\"},", events_size-1);
        }
        if (contains(buf, "Oops")) {
            strncat(events_out, "{\"type\":\"oops\",\"msg\":\"kernel oops\"},", events_size-1);
        }
        if (contains(buf, "avc: denied")) {
            strncat(events_out, "{\"type\":\"selinux_denial\",\"msg\":\"", events_size-1);
            strncat(events_out, buf, events_size-1);
            strncat(events_out, "\"},", events_size-1);
        }
        if (contains(buf, "dm-verity")) {
            strncat(events_out, "{\"type\":\"verity_error\",\"msg\":\"", events_size-1);
            strncat(events_out, buf, events_size-1);
            strncat(events_out, "\"},", events_size-1);
        }
    }
}

/* ── Drift detection ──────────────────────────────────────────────────────── */
static void detect_drift(
    char *new_mods_json, size_t new_size,
    char *removed_mods_json, size_t rem_size,
    char *modified_mods_json, size_t mod_size
) {
    new_mods_json[0] = 0; removed_mods_json[0] = 0; modified_mods_json[0] = 0;

    // Current modules
    char current_names[MAX_MODULES][64];
    int current_count = 0;
    char *mods = rish("cat /proc/modules 2>/dev/null | awk '{print $1}'");
    char *line = strtok(mods, "\n");
    while (line && current_count < MAX_MODULES) {
        strncpy(current_names[current_count], line, 63);
        current_names[current_count][63] = 0;
        current_count++;
        line = strtok(NULL, "\n");
    }

    // Find new modules (in current, not in baseline)
    for (int i = 0; i < current_count; i++) {
        if (!find_in_baseline(current_names[i])) {
            // New module!
            char entry[512];
            snprintf(entry, sizeof(entry),
                "{\"name\":\"%s\",\"detected_at\":\"%ld\"},",
                current_names[i], (long)time(NULL));
            strncat(new_mods_json, entry, new_size-1);
        }
    }

    // Find removed modules (in baseline, not in current)
    for (int i = 0; i < baseline_mod_count; i++) {
        int found = 0;
        for (int j = 0; j < current_count; j++)
            if (strcmp(baseline_mods[i].name, current_names[j]) == 0) { found = 1; break; }
        if (!found) {
            char entry[512];
            snprintf(entry, sizeof(entry),
                "{\"name\":\"%s\",\"removed_at\":\"%ld\"},",
                baseline_mods[i].name, (long)time(NULL));
            strncat(removed_mods_json, entry, rem_size-1);
        }
    }

    // Find modified modules (hash mismatch)
    for (int i = 0; i < current_count; i++) {
        mod_baseline_t *b = find_in_baseline(current_names[i]);
        if (b && strcmp(b->hash_sha256, "UNKNOWN") != 0) {
            char find_cmd[256];
            snprintf(find_cmd, sizeof(find_cmd),
                "find /lib/modules /vendor/lib/modules /system/lib/modules -name '%s.ko' 2>/dev/null | head -1",
                current_names[i]);
            char *path = rish(find_cmd);
            if (path && strlen(path) > 0) {
                char current_hash[65];
                sha256_file(path, current_hash);
                if (strcmp(current_hash, b->hash_sha256) != 0) {
                    char entry[512];
                    snprintf(entry, sizeof(entry),
                        "{\"name\":\"%s\",\"old_hash\":\"%s\",\"new_hash\":\"%s\"},",
                        current_names[i], b->hash_sha256, current_hash);
                    strncat(modified_mods_json, entry, mod_size-1);
                }
            }
        }
    }
}

/* ── Threat correlation ───────────────────────────────────────────────────── */
static int correlate_threats(
    int magisk, int kernelsu, int debugfs,
    int new_mod_count, int modified_mod_count,
    int root_procs, const char *selinux,
    char *threat_json, size_t threat_size
) {
    threat_json[0] = 0;
    int score = 100;
    int confidence = 0;
    char description[256] = "";

    // Correlation rules
    if (magisk && new_mod_count > 0 && debugfs) {
        score = 15; confidence = 95;
        strcpy(description, "Persistence + new kernel code + debug interface = likely active rootkit");
    } else if (kernelsu && modified_mod_count > 0) {
        score = 20; confidence = 90;
        strcpy(description, "KernelSU + modified module = kernel tampering detected");
    } else if (new_mod_count > 0 && !magisk && !kernelsu) {
        score = 60; confidence = 70;
        strcpy(description, "New kernel module without known root manager — investigate");
    } else if (debugfs && root_procs > 5) {
        score = 40; confidence = 80;
        strcpy(description, "debugfs mounted with multiple root processes — attack surface active");
    } else if (!contains(selinux, "Enforcing")) {
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

/* ── JSON output ──────────────────────────────────────────────────────────── */
static void write_json(
    const char *ts, int score, const char *grade,
    int su_found, int magisk, int kernelsu,
    const char *vb_state, const char *vb_mode,
    int nmod, int debugfs, int root_procs,
    const char *selinux, const char *suspicious_mod,
    const char *new_mods, const char *removed_mods,
    const char *modified_mods, const char *kernel_events,
    const char *threat_indicator
) {
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
        "    \"module_count\": %d,\n"
        "    \"established_at\": \"%s\"\n"
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
        "    \"new_modules\": [%s],\n"
        "    \"removed_modules\": [%s],\n"
        "    \"modified_modules\": [%s]\n"
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
        baseline_established ? ctime(&baseline_mods[0].first_seen) : "never",
        score, grade,
        (su_found || magisk || kernelsu) ? "true" : "false",
        su_found, magisk, kernelsu,
        vb_state ? vb_state : "unknown",
        vb_mode ? vb_mode : "unknown",
        selinux,
        (strlen(new_mods) > 0 || strlen(removed_mods) > 0 || strlen(modified_mods) > 0) ? "true" : "false",
        new_mods, removed_mods, modified_mods,
        kernel_events, threat_indicator,
        debugfs ? "true" : "false", root_procs, nmod,
        suspicious_mod ? suspicious_mod : ""
    );

    fflush(f);
    fclose(f);
    fprintf(stderr, "[SHREDDER] JSON written: score=%d grade=%s drift=%s\n",
        score, grade,
        (strlen(new_mods) > 0 || strlen(removed_mods) > 0) ? "YES" : "no");
}

/* ── Main poll ───────────────────────────────────────────────────────────── */
static void poll_integrity(void) {
    int score = 100;

    /* su binaries */
    int su_found = 0;
    const char *su_paths[] = {
        "/sbin/su","/system/bin/su","/system/xbin/su",
        "/su/bin/su","/magisk/.core/bin/su", NULL
    };
    for (int i = 0; su_paths[i]; i++) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "test -e %s && echo yes", su_paths[i]);
        if (contains(rish(cmd), "yes")) { su_found = 1; break; }
    }

    /* Magisk / KernelSU */
    int magisk = contains(rish("test -d /data/adb/magisk && echo yes"), "yes");
    int kernelsu = contains(rish("test -e /sys/kernel/ksu && echo yes"), "yes");

    /* Verified boot */
    char *vb_state = rish("getprop ro.boot.verifiedbootstate");
    char *vb_mode  = rish("getprop ro.boot.flash.locked");
    int vb_ok = contains(vb_state, "green") || contains(vb_state, "yellow");
    if (!vb_ok) score -= 10;

    /* Current modules */
    char *mods = rish("cat /proc/modules 2>/dev/null | wc -l");
    int nmod = mods ? atoi(mods) : 0;

    /* Suspicious module names */
    char suspicious_mod[64] = "";
    char *susp = rish("lsmod 2>/dev/null | grep -iE 'frida|hook|inject|rootkit|backdoor' | head -1 | awk '{print $1}'");
    if (susp && strlen(susp) > 0) strncpy(suspicious_mod, susp, sizeof(suspicious_mod)-1);
    if (suspicious_mod[0]) score -= 20;

    /* debugfs */
    int debugfs = contains(rish("mount 2>/dev/null | grep -c debugfs"), "1");
    if (debugfs) score -= 5;

    /* Root processes */
    char *rp = rish("ps -A 2>/dev/null | awk '$2==\"root\"' | wc -l");
    int root_procs = rp ? atoi(rp) : 0;

    /* SELinux */
    char *se = rish("getenforce 2>/dev/null");
    int enforcing = contains(se, "Enforcing");
    if (!enforcing) score -= 15;
    const char *selinux = enforcing ? "enforcing" : (se && strlen(se) > 0 ? se : "unknown");

    /* Drift detection */
    char new_mods[4096] = "", removed_mods[4096] = "", modified_mods[4096] = "";
    if (baseline_established) {
        detect_drift(new_mods, sizeof(new_mods), removed_mods, sizeof(removed_mods),
                     modified_mods, sizeof(modified_mods));
    }
    int new_mod_count = (strlen(new_mods) > 0) ? 1 : 0;  // Simplified
    int modified_mod_count = (strlen(modified_mods) > 0) ? 1 : 0;

    /* Kernel events from kmsg */
    char kernel_events[4096] = "";
    poll_kmsg(kernel_events, sizeof(kernel_events));

    /* Threat correlation */
    char threat_indicator[512] = "";
    int correlated_score = correlate_threats(magisk, kernelsu, debugfs,
                                              new_mod_count, modified_mod_count,
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
           su_found?"Y":"n", magisk?"Y":"n", kernelsu?"Y":"n",
           vb_state, selinux);
    printf("[SHREDDER]  modules:%d new:%s removed:%s modified:%s\n",
           nmod,
           strlen(new_mods)>0?"YES":"no",
           strlen(removed_mods)>0?"YES":"no",
           strlen(modified_mods)>0?"YES":"no");
    if (strlen(kernel_events) > 0)
        printf("[SHREDDER]  kernel events: %s\n", kernel_events);
    if (strlen(threat_indicator) > 0)
        printf("[SHREDDER]  THREAT: %s\n", threat_indicator);
    fflush(stdout);

    write_json(ts, score, grade, su_found, magisk, kernelsu,
               vb_state ? vb_state : "unknown",
               vb_mode ? vb_mode : "unknown",
               nmod, debugfs, root_procs, selinux, suspicious_mod,
               new_mods, removed_mods, modified_mods, kernel_events, threat_indicator);
}

/* ── Main ─────────────────────────────────────────────────────────────────── */
int main(void) {
    printf("[SHREDDER] v%s Kernel Integrity Daemon: ONLINE\n", VERSION);
    printf("[SHREDDER] Loading baseline...\n");
    load_baseline();

    if (!baseline_established) {
        printf("[SHREDDER] No baseline found. Establishing now...\n");
        establish_baseline();
    }

    printf("[SHREDDER] Initiating kernel message monitor...\n");
    init_kmsg_monitor();

    printf("[SHREDDER] Poll interval: %ds | Deep dive: ACTIVE\n", POLL_SEC);
    fflush(stdout);

    for (;;) {
        poll_integrity();
        sleep(POLL_SEC);
    }
    return 0;
}
