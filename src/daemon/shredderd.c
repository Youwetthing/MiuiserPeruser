/*
 * shredderd v2.2 — Kernel Integrity & Drift Detection Daemon
 * Target: Android 15 / Linux 6.6.89 / MT6768 / SELinux enforcing
 *
 * Fixes from v2.1:
 *   - Hardened module count read (3-retry against bexec() transient empty)
 *   - Fixed ps command (column-safe -o USER,CMD)
 *   - Fixed suspicious module check (proc/modules not lsmod)
 *   - Drift null guard (skip on empty module list)
 *   - Filesystem integrity: pivoted from dmesg (blocked) to /proc/mounts
 *   - AVC denials: acknowledged permanently unavailable (-1 sentinel)
 *   - Kernel hardening: stopped probing blocked proc sys kernel, use uptime
 *   - Duplicate variable declarations eliminated
 *   - Orphaned reboot code block fixed
 *   - sample_module correctly handled (informational, not scored)
 *   - All JSON trailing commas handled
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
#define VERSION         "2.2"
#define POLL_SEC        30
#define RESULTS_FILE    "/data/data/com.termux/files/home/MiuiserPeruser/Registry/daemon_results/shredderd.json"
#define BASELINE_FILE   "/data/data/com.termux/files/home/MiuiserPeruser/data/shredderd_baseline.json"
#define DRIFT_LOG       "/data/data/com.termux/files/home/MiuiserPeruser/data/shredderd_drift.log"
#define MAX_MODULES     512
#define DRIFT_CONFIRM   2

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

static int last_nmod = 0;
static int drift_polls = 0;
static int confirmed_drift = 0;

static unsigned long last_oom_kill = 0;
static unsigned long last_allocstall = 0;
static unsigned long last_pgsteal = 0;
static double last_load_1min = 0.0;
static double last_btime = 0.0;

static int kernel_config_parsed = 0;
static char kernel_config_cache[4096] = "";

static char *rish_read(const char *cmd, char *buf, size_t bufsize) {
    char *result = bexec(cmd);
    if (!result) { buf[0] = 0; return buf; }

    strncpy(buf, result, bufsize - 1);
    buf[bufsize - 1] = 0;
    free(result);

    size_t pos = strlen(buf);
    while (pos > 0 && (buf[pos-1] == '\n' || buf[pos-1] == '\r')) buf[--pos] = 0;

    return buf;
}

static int contains(const char *s, const char *needle) {
    return s && needle && strstr(s, needle) != NULL;
}

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

    static char modbuf[65536];
    char *mods = rish_read("cat /proc/modules | awk '{print $1}'", modbuf, sizeof(modbuf));

    if (!mods || strlen(mods) == 0) {
        fprintf(stderr, "[SHREDDER] Cannot read modules\n");
        return;
    }

    char *line = strtok(mods, "\n");
    while (line && baseline_mod_count < MAX_MODULES) {
        char name[64];
        if (sscanf(line, "%63s", name) == 1 && strlen(name) > 1) {
            mod_baseline_t *m = &baseline_mods[baseline_mod_count];
            strncpy(m->name, name, 63);
            m->size = 0;
            strcpy(m->path, "UNKNOWN");
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

    if (delta == last_nmod - baseline_mod_count) {
        drift_polls++;
    } else {
        drift_polls = 1;
    }
    last_nmod = current_nmod;

    if (drift_polls < DRIFT_CONFIRM) {
        fprintf(stderr, "[SHREDDER] Drift detected (%d modules) but waiting confirmation (poll %d/%d)\n",
                delta, drift_polls, DRIFT_CONFIRM);
        return 0;
    }

    confirmed_drift = 1;

    char current_names[MAX_MODULES][64];
    int current_count = 0;
    char buf[8192];
    char *mods = rish_read("cat /proc/modules 2>/dev/null | awk '{print $1}'", buf, sizeof(buf));
    if (!mods || strlen(mods) == 0) {
        fprintf(stderr, "[SHREDDER] Drift check: module list empty, skipping\n");
        return 0;
    }
    char *line = strtok(mods, "\n");
    while (line && current_count < MAX_MODULES) {
        strncpy(current_names[current_count], line, 63);
        current_names[current_count][63] = 0;
        current_count++;
        line = strtok(NULL, "\n");
    }

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

static void get_selinux_state(char *out, size_t outsize) {
    char buf[256];
    char *se = rish_read("getenforce 2>/dev/null", buf, sizeof(buf));
    if (se && strlen(se) > 0 && !contains(se, "Permission denied")) {
        strncpy(out, se, outsize - 1);
        out[outsize - 1] = 0;
        return;
    }
    char *prop = rish_read("getprop ro.boot.selinux 2>/dev/null", buf, sizeof(buf));
    if (prop && strlen(prop) > 0) {
        strncpy(out, prop, outsize - 1);
        out[outsize - 1] = 0;
        return;
    }
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

static void strip_trailing_comma(char *s) {
    size_t len = strlen(s);
    if (len > 0 && s[len - 1] == ',') {
        s[len - 1] = 0;
    }
}

static void add_reason(char *buf, size_t bufsize, const char *reason, int points) {
    char entry[256];
    snprintf(entry, sizeof(entry), "{\"reason\":\"%s\",\"points\":-%d},", reason, points);
    size_t len = strlen(buf);
    if (len + strlen(entry) < bufsize - 1)
        strncat(buf, entry, bufsize - len - 1);
}

static void check_filesystem_integrity(char *fs_json, size_t fs_size) {
    fs_json[0] = 0;
    char buf[4096];

    char *system_dm = rish_read("cat /proc/mounts 2>/dev/null | grep ' /system ' | grep -c '^/dev/block/dm-'", buf, sizeof(buf));
    char *vendor_dm = rish_read("cat /proc/mounts 2>/dev/null | grep ' /vendor ' | grep -c '^/dev/block/dm-'", buf, sizeof(buf));
    char *system_rw = rish_read("cat /proc/mounts 2>/dev/null | grep ' /system ' | grep -v ' ro ' | wc -l", buf, sizeof(buf));

    int system_on_dm = (system_dm && strcmp(system_dm, "1") == 0);
    int vendor_on_dm = (vendor_dm && strcmp(vendor_dm, "1") == 0);
    int system_not_ro = (system_rw && strcmp(system_rw, "0") != 0);

    if (!system_on_dm || !vendor_on_dm || system_not_ro) {
        char entry[512];
        snprintf(entry, sizeof(entry),
            "{\"type\":\"verity_anomaly\",\"system_on_dm\":%s,\"vendor_on_dm\":%s,\"system_not_ro\":%s},",
            system_on_dm ? "true" : "false",
            vendor_on_dm ? "true" : "false",
            system_not_ro ? "true" : "false");
        strncat(fs_json, entry, fs_size - strlen(fs_json) - 1);
    }
}

static void check_kernel_config(char *json, size_t json_size) {
    if (kernel_config_parsed) {
        strncpy(json, kernel_config_cache, json_size - 1);
        json[json_size - 1] = 0;
        return;
    }

    char buf[8192];
    char *cfg = rish_read("zcat /proc/config.gz 2>/dev/null | grep -E '^CONFIG_(LOCK_DOWN_KERNEL|MODULE_SIG_FORCE|MODULE_SIG_HASH|BPF_SYSCALL|BPF_JIT|KPROBES|UPROBES|FTRACE|LIVEPATCH|KEXEC|SECURITY_SELINUX|SECURITY_SELINUX_DEVELOP|CFI_CLANG|SHADOW_CALL_STACK|ARM64_PTR_AUTH|ARM64_BTI|ARM64_BTI_KERNEL|KASAN|KASAN_HW_TAGS|KFENCE|RETPOLINE|RANDOMIZE_BASE|STRICT_DEVMEM|STRICT_KERNEL_RWX|STRICT_MODULE_RWX|MITIGATE_SPECTRE_BRANCH_HISTORY|HARDLOCKUP_DETECTOR|HUNG_TASK_DETECTOR|WATCHDOG|INIT_STACK_ALL_ZERO|SLUB_DEBUG|RANDOMIZE_MEMORY|CPU_SW_DOMAIN_PAN|ARM64_SW_TTBR0_PAN)'", buf, sizeof(buf));

    int lockdown = contains(cfg, "CONFIG_LOCK_DOWN_KERNEL=y");
    int modsig_force = contains(cfg, "CONFIG_MODULE_SIG_FORCE=y");
    int modsig_sha1 = contains(cfg, "CONFIG_MODULE_SIG_HASH=\"sha1\"");
    int bpf = contains(cfg, "CONFIG_BPF_SYSCALL=y");
    int kprobes = contains(cfg, "CONFIG_KPROBES=y");
    int uprobes = contains(cfg, "CONFIG_UPROBES=y");
    int ftrace = contains(cfg, "CONFIG_FTRACE=y");
    int livepatch = contains(cfg, "CONFIG_LIVEPATCH=y");
    int kexec = contains(cfg, "CONFIG_KEXEC=y");
    int selinux_devel = contains(cfg, "CONFIG_SECURITY_SELINUX_DEVELOP=y");
    int cfi = contains(cfg, "CONFIG_CFI_CLANG=y");
    int scs = contains(cfg, "CONFIG_SHADOW_CALL_STACK=y");
    int pauth = contains(cfg, "CONFIG_ARM64_PTR_AUTH=y");
    int bti = contains(cfg, "CONFIG_ARM64_BTI=y");
    int bti_kernel = contains(cfg, "CONFIG_ARM64_BTI_KERNEL=y");
    int kasan = contains(cfg, "CONFIG_KASAN=y");
    int kasan_hw = contains(cfg, "CONFIG_KASAN_HW_TAGS=y");
    int kfence = contains(cfg, "CONFIG_KFENCE=y");
    int spectre_bhb = contains(cfg, "CONFIG_MITIGATE_SPECTRE_BRANCH_HISTORY=y");
    int hardlockup = contains(cfg, "CONFIG_HARDLOCKUP_DETECTOR=y");
    int hungtask = contains(cfg, "CONFIG_HUNG_TASK_DETECTOR=y");
    int kaslr = contains(cfg, "CONFIG_RANDOMIZE_BASE=y");
    int strict_rwx = contains(cfg, "CONFIG_STRICT_KERNEL_RWX=y");
    int strict_mod_rwx = contains(cfg, "CONFIG_STRICT_MODULE_RWX=y");
    int init_stack_zero = contains(cfg, "CONFIG_INIT_STACK_ALL_ZERO=y");
    int slub_debug = contains(cfg, "CONFIG_SLUB_DEBUG=y");
    int sw_ttbr0_pan = contains(cfg, "CONFIG_ARM64_SW_TTBR0_PAN=y");

    char buf2[64];
    char *mte = rish_read("cat /proc/cpuinfo 2>/dev/null | grep -c ' mte '", buf2, sizeof(buf2));
    int has_mte = (mte && strcmp(mte, "0") != 0);

    snprintf(kernel_config_cache, sizeof(kernel_config_cache),
        "{\"lockdown_lsm\":%s,\"module_sig_force\":%s,\"module_sig_sha1\":%s,"
        "\"bpf\":%s,\"kprobes\":%s,\"uprobes\":%s,\"ftrace\":%s,"
        "\"livepatch\":%s,\"kexec\":%s,\"selinux_devel\":%s,"
        "\"cfi\":%s,\"shadow_call_stack\":%s,\"ptr_auth\":%s,"
        "\"bti_userspace\":%s,\"bti_kernel\":%s,\"kasan\":%s,"
        "\"kasan_hw_tags\":%s,\"cpu_has_mte\":%s,\"kfence\":%s,"
        "\"spectre_bhb\":%s,\"hardlockup_detector\":%s,"
        "\"hungtask_detector\":%s,\"kaslr\":%s,\"strict_kernel_rwx\":%s,"
        "\"strict_module_rwx\":%s,\"init_stack_zero\":%s,"
        "\"slub_debug\":%s,\"sw_ttbr0_pan\":%s,"
        "\"gaps\":[\"no_lockdown_lsm\"%s%s%s%s%s]}",
        lockdown ? "true" : "false",
        modsig_force ? "true" : "false",
        modsig_sha1 ? "true" : "false",
        bpf ? "true" : "false",
        kprobes ? "true" : "false",
        uprobes ? "true" : "false",
        ftrace ? "true" : "false",
        livepatch ? "true" : "false",
        kexec ? "true" : "false",
        selinux_devel ? "true" : "false",
        cfi ? "true" : "false",
        scs ? "true" : "false",
        pauth ? "true" : "false",
        bti ? "true" : "false",
        bti_kernel ? "true" : "false",
        kasan ? "true" : "false",
        kasan_hw ? "true" : "false",
        has_mte ? "true" : "false",
        kfence ? "true" : "false",
        spectre_bhb ? "true" : "false",
        hardlockup ? "true" : "false",
        hungtask ? "true" : "false",
        kaslr ? "true" : "false",
        strict_rwx ? "true" : "false",
        strict_mod_rwx ? "true" : "false",
        init_stack_zero ? "true" : "false",
        slub_debug ? "true" : "false",
        sw_ttbr0_pan ? "true" : "false",
        !modsig_force ? ",\"module_sig_not_forced\"" : "",
        modsig_sha1 ? ",\"sha1_weak_hash\"" : "",
        !hardlockup ? ",\"no_hardlockup_detector\"" : "",
        !hungtask ? ",\"no_hungtask_detector\"" : "",
        kprobes ? ",\"kprobes_compiled\"" : "");

    kernel_config_parsed = 1;
    strncpy(json, kernel_config_cache, json_size - 1);
    json[json_size - 1] = 0;
}

static void check_vm_trends(char *json, size_t json_size) {
    char buf[1024];
    char *vmstat = rish_read("cat /proc/vmstat 2>/dev/null | grep -E '^oom_kill|^allocstall|^pgsteal_kswapd'", buf, sizeof(buf));

    unsigned long oom_kill = 0, allocstall = 0, pgsteal = 0;
    if (vmstat && strlen(vmstat) > 0) {
        char *line = strtok(vmstat, "\n");
        while (line) {
            unsigned long val;
            char key[64];
            if (sscanf(line, "%63s %lu", key, &val) == 2) {
                if (strcmp(key, "oom_kill") == 0) oom_kill = val;
                else if (strncmp(key, "allocstall", 10) == 0) allocstall += val;
                else if (strcmp(key, "pgsteal_kswapd") == 0) pgsteal = val;
            }
            line = strtok(NULL, "\n");
        }
    }

    int oom_delta = (last_oom_kill > 0) ? (int)(oom_kill - last_oom_kill) : 0;
    int alloc_delta = (last_allocstall > 0) ? (int)(allocstall - last_allocstall) : 0;

    snprintf(json, json_size,
        "{\"oom_kill_total\":%lu,\"oom_kill_delta\":%d,"
        "\"allocstall_total\":%lu,\"allocstall_delta\":%d,"
        "\"pgsteal_kswapd\":%lu,\"trending\":\"%s\"}",
        oom_kill, oom_delta,
        allocstall, alloc_delta,
        pgsteal,
        (oom_delta > 0) ? "OOM_KILL_DETECTED" :
        (alloc_delta > 1000) ? "HIGH_MEMORY_PRESSURE" : "stable");

    last_oom_kill = oom_kill;
    last_allocstall = allocstall;
    last_pgsteal = pgsteal;
}

static void check_load_pressure(char *json, size_t json_size) {
    char buf[128];
    char *load = rish_read("cat /proc/loadavg 2>/dev/null | awk '{print $1}'", buf, sizeof(buf));
    double load_1min = (load && strlen(load) > 0) ? atof(load) : -1.0;

    double delta = (last_load_1min > 0) ? (load_1min - last_load_1min) : 0.0;

    snprintf(json, json_size,
        "{\"load_1min\":%.2f,\"load_delta\":%.2f,\"anomaly\":\"%s\"}",
        load_1min, delta,
        (delta > 10.0) ? "SPIKE" :
        (load_1min > 20.0) ? "HIGH" :
        (load_1min > 10.0) ? "ELEVATED" : "normal");

    last_load_1min = load_1min;
}

static int check_reboot(void) {
    char buf[256];
    char *stat = rish_read("cat /proc/stat 2>/dev/null | grep '^btime'", buf, sizeof(buf));
    double btime = 0.0;
    if (stat && sscanf(stat, "btime %lf", &btime) == 1) {
        if (last_btime > 0.0 && btime != last_btime) {
            last_btime = btime;
            return 1;
        }
        last_btime = btime;
    }
    return 0;
}

static void check_nmi_watchdog(char *json, size_t json_size) {
    char buf[256];
    char *nmi = rish_read("cat /proc/interrupts 2>/dev/null | grep -i nmi | wc -l", buf, sizeof(buf));
    int nmi_present = (nmi && strcmp(nmi, "0") != 0);

    snprintf(json, json_size,
        "{\"nmi_watchdog_active\":%s,\"assessment\":\"%s\"}",
        nmi_present ? "true" : "false",
        nmi_present ? "hardlockup detector active" : "hardlockup detector disabled or not configured");
}

static void check_module_surface(char *json, size_t json_size, int nmod,
                                  int *out_sample_module_found, int *out_untracked_count) {
    int untracked_count = 0;
    char untracked_buf[1024] = "";

    char lbuf[16384];
    char *lsmod_out = rish_read("lsmod 2>/dev/null", lbuf, sizeof(lbuf));
    int lsmod_count = 0;
    int permanent_count = 0;
    int sample_module_found = 0;
    if (lsmod_out && strlen(lsmod_out) > 0) {
        char *line = strtok(lsmod_out, "\n");
        if (line) line = strtok(NULL, "\n");
        while (line) {
            char name[64] = "";
            sscanf(line, "%63s", name);
            if (strlen(name) > 0) {
                lsmod_count++;
                if (strcmp(name, "sample_module") == 0) sample_module_found = 1;

                if (baseline_established && !find_in_baseline(name)) {
                    untracked_count++;
                    char entry[80];
                    snprintf(entry, sizeof(entry), "\"%s\",", name);
                    size_t ulen = strlen(untracked_buf);
                    if (ulen + strlen(entry) < sizeof(untracked_buf) - 1)
                        strncat(untracked_buf, entry, sizeof(untracked_buf) - ulen - 1);
                }
            }
            if (strstr(line, "[permanent]")) permanent_count++;
            line = strtok(NULL, "\n");
        }
    }
    strip_trailing_comma(untracked_buf);

    *out_sample_module_found = sample_module_found;
    *out_untracked_count = untracked_count;

    snprintf(json, json_size,
        "{\"sys_module_readable\":false,\"proc_modules_count\":%d,"
        "\"permanent_module_count\":%d,\"sample_module_present\":%s,"
        "\"untracked_modules\":[%s]}",
        nmod, permanent_count,
        sample_module_found ? "true" : "false", untracked_buf);
}

static void check_kernel_hardening_gaps(char *json, size_t json_size) {
    char buf[256];
    char *uptime = rish_read("cat /proc/uptime 2>/dev/null | awk '{print $1}'", buf, sizeof(buf));
    double uptime_sec = (uptime && strlen(uptime) > 0) ? atof(uptime) : -1.0;

    snprintf(json, json_size,
        "{\"kernel_sysctls_blocked\":true,\"uptime_sec\":%.2f,"
        "\"assessment\":\"proc sys kernel blocked by SELinux -- expected on hardened builds\"}",
        uptime_sec);
}

static void check_kernel_build(char *json, size_t json_size) {
    char buf[1024];
    char *ver = rish_read("cat /proc/version 2>/dev/null", buf, sizeof(buf));
    char kernel_ver[64] = "unknown";
    char build_info[64] = "unknown";
    char clang_ver[32] = "unknown";
    int has_lto = 0, has_bolt = 0, has_pgo = 0, has_mlgo = 0;

    if (ver && strlen(ver) > 0) {
        sscanf(ver, "Linux version %63s", kernel_ver);
        has_lto  = contains(ver, "+lto");
        has_bolt = contains(ver, "+bolt");
        has_pgo  = contains(ver, "+pgo");
        has_mlgo = contains(ver, "+mlgo");

        char *clang = strstr(ver, "clang version ");
        if (clang) sscanf(clang + strlen("clang version "), "%31s", clang_ver);

        char *hash = strrchr(ver, '#');
        if (hash) {
            strncpy(build_info, hash, sizeof(build_info) - 1);
            build_info[sizeof(build_info) - 1] = 0;
        }
    }

    snprintf(json, json_size,
        "{\"kernel_version\":\"%.63s\",\"clang_version\":\"%.31s\","
        "\"build_info\":\"%.63s\",\"lto\":%s,\"bolt\":%s,\"pgo\":%s,\"mlgo\":%s}",
        kernel_ver, clang_ver, build_info,
        has_lto ? "true" : "false", has_bolt ? "true" : "false",
        has_pgo ? "true" : "false", has_mlgo ? "true" : "false");
}

static void check_build_integrity(char *json, size_t json_size, int *out_inconsistent) {
    char buf[256];
    char *tags = rish_read("getprop ro.build.tags 2>/dev/null", buf, sizeof(buf));
    char tags_copy[64];
    strncpy(tags_copy, (tags && strlen(tags) > 0) ? tags : "unknown", sizeof(tags_copy) - 1);
    tags_copy[sizeof(tags_copy) - 1] = 0;

    char buf2[256];
    char *type = rish_read("getprop ro.build.type 2>/dev/null", buf2, sizeof(buf2));
    char type_copy[32];
    strncpy(type_copy, (type && strlen(type) > 0) ? type : "unknown", sizeof(type_copy) - 1);
    type_copy[sizeof(type_copy) - 1] = 0;

    char buf3[512];
    char *fp = rish_read("getprop ro.build.fingerprint 2>/dev/null", buf3, sizeof(buf3));
    char fp_copy[300];
    strncpy(fp_copy, (fp && strlen(fp) > 0) ? fp : "unknown", sizeof(fp_copy) - 1);
    fp_copy[sizeof(fp_copy) - 1] = 0;

    int consistent = (strcmp(tags_copy, "unknown") != 0 && strstr(fp_copy, tags_copy) != NULL);
    int prod_build = (strcmp(type_copy, "user") == 0 && strcmp(tags_copy, "release-keys") == 0);

    *out_inconsistent = !consistent;

    snprintf(json, json_size,
        "{\"tags\":\"%.63s\",\"type\":\"%.31s\",\"fingerprint\":\"%.255s\","
        "\"consistent\":%s,\"production_build\":%s}",
        tags_copy, type_copy, fp_copy,
        consistent ? "true" : "false", prod_build ? "true" : "false");
}

static int check_avc_denials(void) {
    return -1;
}

static void check_adb_state(char *json, size_t json_size) {
    char buf[128];
    char *adbd = rish_read("getprop init.svc.adbd 2>/dev/null", buf, sizeof(buf));
    char buf2[128];
    char *usbcfg = rish_read("getprop persist.sys.usb.config 2>/dev/null", buf2, sizeof(buf2));

    snprintf(json, json_size,
        "{\"adbd_running\":%s,\"usb_config\":\"%.63s\","
        "\"note\":\"informational only -- not scored, expected on dev devices\"}",
        contains(adbd, "running") ? "true" : "false",
        (usbcfg && strlen(usbcfg) > 0) ? usbcfg : "unknown");
}

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
        strcpy(description, "New kernel module without known root manager -- investigate");
    } else if (debugfs && root_procs > 5) {
        score = 40; confidence = 80;
        strcpy(description, "debugfs mounted with multiple root processes -- attack surface active");
    } else if (!contains(selinux, "enforcing") && !contains(selinux, "Enforcing")) {
        score = 30; confidence = 85;
        strcpy(description, "SELinux not enforcing -- MAC bypass possible");
    }

    if (confidence > 0) {
        snprintf(threat_json, threat_size,
            "{\"type\":\"correlated\",\"confidence\":%d,\"score\":%d,\"description\":\"%s\"}",
            confidence, score, description);
    }
    return score;
}

static void write_json(
    const char *ts, int score, const char *grade,
    int su_found, int magisk, int kernelsu,
    const char *vb_state, const char *vb_mode,
    int nmod, int debugfs, int root_procs,
    const char *selinux, const char *suspicious_mod,
    const char *new_mods, const char *removed_mods,
    const char *kernel_events, const char *fs_events, const char *threat_indicator,
    const char *module_surface_json, const char *kernel_hardening_json,
    const char *kernel_build_json, const char *build_integrity_json,
    int avc_denials, const char *adb_state_json,
    const char *kernel_config_json, const char *vm_trends_json,
    const char *load_pressure_json, const char *nmi_watchdog_json,
    const char *score_reasons_json, const char *raw_reads_json)
{
    char new_clean[4096], rem_clean[4096], evt_clean[4096], fs_clean[4096], reasons_clean[3072];
    strncpy(new_clean, new_mods, sizeof(new_clean) - 1);
    strncpy(rem_clean, removed_mods, sizeof(rem_clean) - 1);
    strncpy(evt_clean, kernel_events, sizeof(evt_clean) - 1);
    strncpy(fs_clean, fs_events, sizeof(fs_clean) - 1);
    strncpy(reasons_clean, score_reasons_json, sizeof(reasons_clean) - 1);
    new_clean[sizeof(new_clean) - 1] = 0;
    rem_clean[sizeof(rem_clean) - 1] = 0;
    evt_clean[sizeof(evt_clean) - 1] = 0;
    fs_clean[sizeof(fs_clean) - 1] = 0;
    reasons_clean[sizeof(reasons_clean) - 1] = 0;
    strip_trailing_comma(new_clean);
    strip_trailing_comma(rem_clean);
    strip_trailing_comma(evt_clean);
    strip_trailing_comma(fs_clean);
    strip_trailing_comma(reasons_clean);

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
        "  \"filesystem_integrity\": [%s],\n\n"
        "  \"threat_indicators\": [%s],\n\n"
        "  \"score_reasons\": [%s],\n\n"
        "  \"raw_reads\": %s,\n\n"
        "  \"module_surface\": %s,\n\n"
        "  \"kernel_hardening_gaps\": %s,\n\n"
        "  \"kernel_build\": %s,\n\n"
        "  \"build_integrity\": %s,\n\n"
        "  \"selinux_avc_denials\": %d,\n\n"
        "  \"adb_state\": %s,\n\n"
        "  \"kernel_config\": %s,\n\n"
        "  \"vm_trends\": %s,\n\n"
        "  \"load_pressure\": %s,\n\n"
        "  \"nmi_watchdog\": %s,\n\n"
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
        evt_clean, fs_clean, threat_indicator,
        reasons_clean, raw_reads_json,
        module_surface_json, kernel_hardening_json, kernel_build_json,
        build_integrity_json, avc_denials, adb_state_json,
        kernel_config_json, vm_trends_json, load_pressure_json, nmi_watchdog_json,
        debugfs ? "true" : "false", root_procs, nmod,
        suspicious_mod ? suspicious_mod : "");

    fflush(f);
    fclose(f);
    fprintf(stderr, "[SHREDDER] JSON written: score=%d grade=%s drift_confirmed=%s\n",
        score, grade, confirmed_drift ? "YES" : "no");
}

static void poll_integrity(void) {
    int score = 100;
    char score_reasons[3072] = "";
    char raw_reads_json[1024] = "";
    char buf[8192];

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

    char magisk_buf[32];
    char *magisk_res = rish_read("test -d /data/adb/magisk && echo yes", magisk_buf, sizeof(magisk_buf));
    int magisk = contains(magisk_res, "yes");
    char ksu_buf[32];
    char *ksu_res = rish_read("test -e /sys/kernel/ksu && echo yes", ksu_buf, sizeof(ksu_buf));
    int kernelsu = contains(ksu_res, "yes");

    char vb_state_buf[128], vb_mode_buf[128];
    char *vb_state = rish_read("getprop ro.boot.verifiedbootstate", vb_state_buf, sizeof(vb_state_buf));
    char *vb_mode  = rish_read("getprop ro.boot.flash.locked", vb_mode_buf, sizeof(vb_mode_buf));
    int vb_ok = contains(vb_state, "green") || contains(vb_state, "yellow");
    if (!vb_ok) {
        add_reason(score_reasons, sizeof(score_reasons), "verified_boot_state not green/yellow", 10);
        score -= 10;
    }

    int nmod = 0;
    for (int retry = 0; retry < 3; retry++) {
        char *mods = rish_read("cat /proc/modules 2>/dev/null | wc -l", buf, sizeof(buf));
        if (mods && strlen(mods) > 0) {
            nmod = atoi(mods);
            if (nmod > 0) break;
        }
        fprintf(stderr, "[SHREDDER] Module count empty (attempt %d/3)\n", retry + 1);
        if (retry < 2) sleep(2);
    }
    if (nmod <= 0) {
        fprintf(stderr, "[SHREDDER] FATAL: module count unreadable, skipping poll\n");
        return;
    }

    char suspicious_mod[64] = "";
    char *susp = rish_read("cat /proc/modules 2>/dev/null | awk '{print $1}' | grep -iE 'frida|hook|inject|rootkit|backdoor' | head -1", buf, sizeof(buf));
    if (susp && strlen(susp) > 0) strncpy(suspicious_mod, susp, sizeof(suspicious_mod) - 1);
    if (suspicious_mod[0]) {
        add_reason(score_reasons, sizeof(score_reasons), "suspicious module name matched frida/hook/inject/rootkit/backdoor pattern", 20);
        score -= 20;
    }

    char dfs_buf[32];
    char *dfs = rish_read("mount 2>/dev/null | grep -c debugfs", dfs_buf, sizeof(dfs_buf));
    int debugfs = contains(dfs, "1");
    if (debugfs) {
        add_reason(score_reasons, sizeof(score_reasons), "debugfs mounted", 5);
        score -= 5;
    }

    char rp_buf[32];
    char *rp = rish_read("ps -A -o USER,CMD 2>/dev/null | grep -c '^root'", rp_buf, sizeof(rp_buf));
    int root_procs = (rp && strlen(rp) > 0) ? atoi(rp) : 0;

    char selinux[32];
    get_selinux_state(selinux, sizeof(selinux));
    int enforcing = contains(selinux, "enforcing") || contains(selinux, "Enforcing");
    if (!enforcing) {
        add_reason(score_reasons, sizeof(score_reasons), "SELinux not enforcing", 15);
        score -= 15;
    }

    snprintf(raw_reads_json, sizeof(raw_reads_json),
        "{\"verifiedbootstate\":\"%s\",\"flash_locked\":\"%s\",\"selinux_getenforce\":\"%s\","
        "\"magisk_dir_check\":\"%s\",\"kernelsu_check\":\"%s\",\"debugfs_grep_count\":\"%s\","
        "\"root_proc_grep_count\":\"%s\"}",
        vb_state ? vb_state : "", vb_mode ? vb_mode : "", selinux,
        magisk_res ? magisk_res : "", ksu_res ? ksu_res : "",
        dfs ? dfs : "", rp ? rp : "");

    char new_mods[4096] = "", removed_mods[4096] = "";
    int drift_delta = 0;
    if (baseline_established) {
        drift_delta = detect_drift(nmod, new_mods, sizeof(new_mods),
                                   removed_mods, sizeof(removed_mods));
    }

    char kernel_events[4096] = "";
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

    char fs_json[4096] = "";
    check_filesystem_integrity(fs_json, sizeof(fs_json));

    char threat_indicator[512] = "";
    int correlated_score = correlate_threats(magisk, kernelsu, debugfs,
                                              confirmed_drift, drift_delta,
                                              root_procs, selinux,
                                              threat_indicator, sizeof(threat_indicator));
    if (correlated_score < score) score = correlated_score;

    char kernel_config_j[4096] = "";
    char vm_trends_j[512] = "";
    char load_pressure_j[256] = "";
    char nmi_watchdog_j[256] = "";

    char module_surface_json[2048] = "";
    int sample_module_found = 0, untracked_count = 0;
    check_module_surface(module_surface_json, sizeof(module_surface_json), nmod,
                          &sample_module_found, &untracked_count);
    if (untracked_count > 0) {
        add_reason(score_reasons, sizeof(score_reasons), "untracked kernel modules present (loaded but not in baseline)", 10);
        score -= 10;
    }

    char kernel_hardening_json[512] = "";
    check_kernel_hardening_gaps(kernel_hardening_json, sizeof(kernel_hardening_json));

    char kernel_build_json[512] = "";
    check_kernel_build(kernel_build_json, sizeof(kernel_build_json));

    int build_inconsistent = 0;
    char build_integrity_json[768] = "";
    check_build_integrity(build_integrity_json, sizeof(build_integrity_json), &build_inconsistent);
    if (build_inconsistent) {
        add_reason(score_reasons, sizeof(score_reasons), "build fingerprint/tags inconsistent", 25);
        score -= 25;
    }

    check_kernel_config(kernel_config_j, sizeof(kernel_config_j));
    check_vm_trends(vm_trends_j, sizeof(vm_trends_j));
    check_load_pressure(load_pressure_j, sizeof(load_pressure_j));
    check_nmi_watchdog(nmi_watchdog_j, sizeof(nmi_watchdog_j));

    int rebooted = check_reboot();
    if (rebooted) {
        add_reason(score_reasons, sizeof(score_reasons), "kernel reboot detected (btime changed)", 20);
        score -= 20;
        gaveld_emit(DAEMON_NAME, "REBOOT_DETECTED", 0.0, "Kernel reboot detected via btime change");
    }

    if (contains(kernel_config_j, "\"lockdown_lsm\":false")) {
        add_reason(score_reasons, sizeof(score_reasons), "kernel lockdown LSM not enabled", 10);
        score -= 10;
    }
    if (contains(kernel_config_j, "\"module_sig_force\":false")) {
        add_reason(score_reasons, sizeof(score_reasons), "module signature enforcement not forced", 10);
        score -= 10;
    }
    if (contains(kernel_config_j, "\"module_sig_sha1\":true")) {
        add_reason(score_reasons, sizeof(score_reasons), "module signature hash uses weak SHA1", 5);
        score -= 5;
    }
    if (contains(kernel_config_j, "\"hardlockup_detector\":false")) {
        add_reason(score_reasons, sizeof(score_reasons), "hardlockup detector not present in kernel config", 5);
        score -= 5;
    }
    if (contains(kernel_config_j, "\"hungtask_detector\":false")) {
        add_reason(score_reasons, sizeof(score_reasons), "hung-task detector not present in kernel config", 5);
        score -= 5;
    }
    if (contains(kernel_config_j, "\"kprobes\":true")) {
        add_reason(score_reasons, sizeof(score_reasons), "kprobes compiled into kernel (function-hooking capability present)", 5);
        score -= 5;
    }
    if (contains(kernel_config_j, "\"uprobes\":true")) {
        add_reason(score_reasons, sizeof(score_reasons), "uprobes compiled into kernel (userspace-hooking capability present)", 5);
        score -= 5;
    }

    int avc_denials = check_avc_denials();

    char adb_state_json[512] = "";
    check_adb_state(adb_state_json, sizeof(adb_state_json));

    if (score < 0) score = 0;
    const char *grade = score >= 90 ? "INTACT"
                      : score >= 70 ? "DEGRADED"
                      : score >= 50 ? "AT_RISK"
                      : "CRITICAL";

    time_t t = time(NULL);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", localtime(&t));

    printf("[SHREDDER] -- %s -- score=%d [%s] --\n", ts, score, grade);
    printf("[SHREDDER]  su:%s magisk:%s ksu:%s | vb:%s | selinux:%s\n",
           su_found ? "Y" : "n", magisk ? "Y" : "n", kernelsu ? "Y" : "n",
           vb_state, selinux);
    printf("[SHREDDER]  modules:%d baseline:%d drift:%s confirmed:%s\n",
           nmod, baseline_mod_count,
           (drift_delta != 0) ? "YES" : "no",
           confirmed_drift ? "YES" : "no");
    if (strlen(threat_indicator) > 0)
        printf("[SHREDDER]  THREAT: %s\n", threat_indicator);
    if (strlen(score_reasons) > 0) {
        char reasons_print[3072];
        strncpy(reasons_print, score_reasons, sizeof(reasons_print) - 1);
        reasons_print[sizeof(reasons_print) - 1] = 0;
        strip_trailing_comma(reasons_print);
        printf("[SHREDDER]  SCORE REASONS: %s\n", reasons_print);
    }
    fflush(stdout);

    if (score < 50 || (confirmed_drift && drift_delta > 0)) {
        gaveld_emit(DAEMON_NAME, "KERNEL_THREAT", 0.0, threat_indicator);
        if (confirmed_drift && drift_delta > 0)
            gaveld_emit(DAEMON_NAME, "NEW_KERNEL_MODULE", 0.0, new_mods);
    }

    write_json(ts, score, grade, su_found, magisk, kernelsu,
               vb_state ? vb_state : "unknown",
               vb_mode ? vb_mode : "unknown",
               nmod, debugfs, root_procs, selinux, suspicious_mod,
               new_mods, removed_mods, kernel_events, fs_json, threat_indicator,
               module_surface_json, kernel_hardening_json, kernel_build_json,
               build_integrity_json, avc_denials, adb_state_json,
               kernel_config_j, vm_trends_j, load_pressure_j, nmi_watchdog_j,
               score_reasons, raw_reads_json);
}

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