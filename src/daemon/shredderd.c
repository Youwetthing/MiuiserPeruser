/*
 * shredderd v2.3 — Kernel Integrity & Drift Detection Daemon
 * Target: Android 15 / Linux 6.6.89 / MT6768 / SELinux enforcing
 *
 * Fixes from v2.2:
 *   - Batched probe layer. poll_integrity() previously fired ~20
 *     individual bexec() calls per poll for small facts (su paths x5,
 *     magisk, ksu, vb_state, vb_mode, getenforce, debugfs, root procs,
 *     adb state, build tags/type/fp, suspicious module grep, lsmod,
 *     kernel version, vmstat, loadavg, btime, nmi, uptime, 3x mount
 *     checks, dmesg tail). Same root cause already fixed in tigerclawd.
 *     All now issued as FOUR combined bexec() calls per poll (FAST/
 *     MODULES/KERNEL/HEAVY), echo-delimited and parsed locally via
 *     probe_section(), each independently budget-checked before firing.
 *   - nmod's 3-retry loop, detect_drift()'s full module-list read,
 *     establish_baseline()'s full module-list read, and
 *     check_kernel_config()'s cached zcat/regex read are all
 *     intentionally NOT folded into the batch: nmod's retry needs to
 *     fire conditionally with a 2s backoff between attempts (doesn't
 *     fold into a single call); the drift/baseline full-list reads are
 *     comparatively large and only needed conditionally; kernel_config
 *     is parsed once and cached (kernel_config_parsed) for the life of
 *     the process, so re-fetching it every poll inside the batch would
 *     tax every single poll for a value only the first poll needs.
 *   - probe_copy(tag, buf, bufsize) mirrors rish_read()'s signature and
 *     copies a probe section straight into the CALLER's own buffer,
 *     rather than returning a pointer into a single shared scratch
 *     buffer (the design tigerclawd uses). poll_integrity() here holds
 *     several probe values concurrently (vb_state, vb_mode, selinux,
 *     magisk_res, ksu_res, dfs, rp are all still alive together at
 *     write_json() time) -- a shared scratch buffer would silently
 *     corrupt earlier reads the moment a later probe_section() call
 *     runs, so each call site gets its own storage immediately instead.
 *   - Bonus fix found during this rewrite, unrelated to batching:
 *     check_filesystem_integrity() previously read system_dm/vendor_dm/
 *     system_rw via three sequential rish_read() calls into the SAME
 *     shared `char buf[4096]` -- since rish_read() writes into and
 *     returns that same buffer, all three pointers aliased one another,
 *     so by evaluation time all three checks were actually comparing
 *     against whatever the LAST call (system_rw) had written. Each now
 *     gets its own separate buffer.
 */

#include "ipc_globals.h"
#include "backend_exec.h"
#include "gaveld_emit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>
#include <fcntl.h>

static void slog(const char *level, const char *fmt, ...);
static void splinterd_emit(const char *type, const char *payload);
#include <errno.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <openssl/sha.h>
#include <stdbool.h>

#define DAEMON_NAME     "shredderd"
#define VERSION         "2.3"
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

/* ── Batched probe layer ──────────────────────────────────────────────────
 * See v2.3 changelog above. Sections are pulled via probe_copy(), which
 * copies into the caller's own buffer immediately -- g_probe_buf itself
 * is never returned directly, so section read order never matters and
 * concurrent held values never collide (see changelog note on why this
 * differs from tigerclawd's shared-scratch probe_section()).
 * ───────────────────────────────────────────────────────────────────────── */
#define SD_PROBE_BUF_SIZE 131072
static char g_probe_buf[SD_PROBE_BUF_SIZE] = "";
static int  g_probe_loaded = 0;

#define SD_PROBE_CMD_BUDGET 1450

/* Fixed 2026-07-20: this was the SAME destructive-truncation bug already
 * found and fixed in tigerclawd's probe_section() earlier this session --
 * null-terminating directly inside g_probe_buf, which permanently
 * truncates it the moment any tag is read, destroying every tag still
 * downstream regardless of whether it's been read yet. Re-copied the old
 * pattern into this file by mistake instead of the fixed one. Confirmed
 * live: SUCHECK being read first truncated everything after it, so every
 * other tag in this poll was silently reading empty -- masked because
 * magisk/ksu/su's "empty" and "correctly absent" look identical, and
 * because get_selinux_state()'s own standalone fallback happened to paper
 * over GETENFORCE being empty too. Fix: copy into a scratch buffer
 * instead of mutating g_probe_buf, exactly as tigerclawd now does. */
#define SD_SECTION_SCRATCH_SIZE 65536
static char g_section_scratch[SD_SECTION_SCRATCH_SIZE];

static char *probe_section_raw(const char *tag) {
    if (!g_probe_loaded) return NULL;
    char marker[64];
    snprintf(marker, sizeof(marker), "==%s==", tag);
    char *start = strstr(g_probe_buf, marker);
    if (!start) return NULL;
    start += strlen(marker);
    while (*start == '\r' || *start == '\n') start++;
    /* Empty section (command produced zero output): skipping the echo's
     * own trailing newline lands directly on the NEXT tag's opening "==".
     * Without this check, the search below would scan past it for the
     * following boundary and silently return the next section's entire
     * content as if it belonged to this one -- confirmed live 2026-07-20:
     * SUSPMOD (genuinely empty) absorbed all of LSMOD's output this way,
     * producing a false suspicious-module hit since add_reason() only
     * checks for non-empty, not actual match content. */
    if (start[0] == '=' && start[1] == '=') {
        g_section_scratch[0] = '\0';
        return g_section_scratch;
    }
    char *end = strstr(start, "\n==");
    size_t len = end ? (size_t)(end - start) : strlen(start);
    if (len >= SD_SECTION_SCRATCH_SIZE) len = SD_SECTION_SCRATCH_SIZE - 1;
    memcpy(g_section_scratch, start, len);
    g_section_scratch[len] = '\0';
    size_t pos = len;
    while (pos > 0 && (g_section_scratch[pos-1] == '\n' || g_section_scratch[pos-1] == '\r'))
        g_section_scratch[--pos] = '\0';
    return g_section_scratch;
}

/* Mirrors rish_read()'s signature -- copies the found section straight
 * into the caller's buffer instead of returning a pointer into shared
 * storage. Safe to call any number of times in any order per poll. */
static char *probe_copy(const char *tag, char *buf, size_t bufsize) {
    char *v = probe_section_raw(tag);
    if (!v) { buf[0] = 0; return buf; }
    strncpy(buf, v, bufsize - 1);
    buf[bufsize - 1] = 0;
    return buf;
}

static int append_probe_chunk(const char *tag, const char *cmd) {
    size_t cmdlen = strlen(cmd);
    if (cmdlen > SD_PROBE_CMD_BUDGET) {
        char errmsg[160];
        snprintf(errmsg, sizeof(errmsg),
            "probe chunk %s %zu bytes exceeds %d-byte budget, refusing (would silently truncate)",
            tag, cmdlen, SD_PROBE_CMD_BUDGET);
        slog("ERROR", "%s", errmsg);
        return -1;
    }

    char *raw = bexec(cmd);
    if (!raw) return -1;

    size_t curlen = strlen(g_probe_buf);
    size_t rawlen = strlen(raw);
    size_t space = sizeof(g_probe_buf) - curlen - 1;
    if (rawlen > space) {
        slog("ERROR",
            "probe chunk %s response %zu bytes exceeds remaining buffer space %zu, truncating",
            tag, rawlen, space);
        rawlen = space;
    }
    memcpy(g_probe_buf + curlen, raw, rawlen);
    g_probe_buf[curlen + rawlen] = '\0';
    free(raw);
    return 0;
}

static void load_probe_data(void) {
    g_probe_loaded = 0;
    g_probe_buf[0] = '\0';

    static const char *cmd_fast =
        "echo ==SUCHECK==; test -e /sbin/su -o -e /system/bin/su -o -e /system/xbin/su -o -e /su/bin/su -o -e /magisk/.core/bin/su && echo yes;"
        "echo ==MAGISK==; test -d /data/adb/magisk && echo yes;"
        "echo ==KSU==; test -e /sys/kernel/ksu && echo yes;"
        "echo ==VBSTATE==; getprop ro.boot.verifiedbootstate 2>/dev/null;"
        "echo ==VBMODE==; getprop ro.boot.flash.locked 2>/dev/null;"
        "echo ==GETENFORCE==; getenforce 2>/dev/null;"
        "echo ==DEBUGFS==; mount 2>/dev/null | grep -c debugfs;"
        "echo ==ROOTPROCS==; ps -A -o USER,CMD 2>/dev/null | grep -c '^root';"
        "echo ==ADBD==; getprop init.svc.adbd 2>/dev/null;"
        "echo ==USBCFG==; getprop persist.sys.usb.config 2>/dev/null;"
        "echo ==BUILDTAGS==; getprop ro.build.tags 2>/dev/null;"
        "echo ==BUILDTYPE==; getprop ro.build.type 2>/dev/null;"
        "echo ==BUILDFP==; getprop ro.build.fingerprint 2>/dev/null;";

    static const char *cmd_modules =
        "echo ==SUSPMOD==; cat /proc/modules 2>/dev/null | awk '{print $1}' | grep -iE 'frida|hook|inject|rootkit|backdoor' | head -1;"
        "echo ==LSMOD==; lsmod 2>/dev/null;"
        "echo ==MODCOUNT==; cat /proc/modules 2>/dev/null | wc -l;";

    static const char *cmd_kernel =
        "echo ==PROCVERSION==; cat /proc/version 2>/dev/null;"
        "echo ==VMSTAT==; cat /proc/vmstat 2>/dev/null | grep -E '^oom_kill|^allocstall|^pgsteal_kswapd';"
        "echo ==LOADAVG==; cat /proc/loadavg 2>/dev/null | awk '{print $1}';"
        "echo ==BTIME==; cat /proc/stat 2>/dev/null | grep '^btime';"
        "echo ==NMI==; cat /proc/interrupts 2>/dev/null | grep -i nmi | wc -l;"
        "echo ==UPTIME==; cat /proc/uptime 2>/dev/null | awk '{print $1}';";

    static const char *cmd_heavy =
        "echo ==SYSDM==; cat /proc/mounts 2>/dev/null | grep ' /system ' | grep -c '^/dev/block/dm-';"
        "echo ==VENDORDM==; cat /proc/mounts 2>/dev/null | grep ' /vendor ' | grep -c '^/dev/block/dm-';"
        "echo ==SYSRW==; cat /proc/mounts 2>/dev/null | grep ' /system ' | grep -v ' ro ' | wc -l;"
        "echo ==DMESG==; dmesg 2>/dev/null | tail -20 | grep -E 'module|Oops|avc|verity' | head -5;";

    if (append_probe_chunk("FAST", cmd_fast) != 0) return;
    if (append_probe_chunk("MODULES", cmd_modules) != 0) return;
    if (append_probe_chunk("KERNEL", cmd_kernel) != 0) return;
    if (append_probe_chunk("HEAVY", cmd_heavy) != 0) return;

    g_probe_loaded = 1;
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
    slog("INFO", "Baseline loaded: %d modules", baseline_mod_count);
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

/* Standalone -- not batched. Rare (once per fresh install), and needs
 * the full module list rather than just the count. */
static void establish_baseline(void) {
    slog("INFO", "Establishing baseline...");
    baseline_mod_count = 0;

    static char modbuf[65536];
    char *mods = rish_read("cat /proc/modules | awk '{print $1}'", modbuf, sizeof(modbuf));

    if (!mods || strlen(mods) == 0) {
        slog("ERROR", "Cannot read modules");
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
    slog("INFO", "Baseline established: %d modules", baseline_mod_count);
}

static mod_baseline_t* find_in_baseline(const char *name) {
    for (int i = 0; i < baseline_mod_count; i++)
        if (strcmp(baseline_mods[i].name, name) == 0)
            return &baseline_mods[i];
    return NULL;
}

/* Standalone -- not batched. Only fires when nmod's delta already
 * suggests drift; needs the full module list, not just the count. */
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
        slog("WARN", "Drift detected (%d modules) but waiting confirmation (poll %d/%d)",
                delta, drift_polls, DRIFT_CONFIRM);
        return 0;
    }

    confirmed_drift = 1;

    char current_names[MAX_MODULES][64];
    int current_count = 0;
    char buf[8192];
    char *mods = rish_read("cat /proc/modules 2>/dev/null | awk '{print $1}'", buf, sizeof(buf));
    if (!mods || strlen(mods) == 0) {
        slog("WARN", "Drift check: module list empty, skipping");
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

/* Reads GETENFORCE from the FAST probe chunk. Fallback chain (ro.boot.selinux
 * property, then /sys/fs/selinux/enforce) stays standalone -- only fires if
 * the batched read was empty/blocked, same conditional-fallback pattern used
 * elsewhere in this fleet. */
static void get_selinux_state(char *out, size_t outsize) {
    char buf[256];
    char *se = probe_copy("GETENFORCE", buf, sizeof(buf));
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

/* Neutralize chars that would break JSON string values if raw device
 * output (dmesg lines, etc.) ever contains a quote/backslash/control
 * char. Same helper as fugitoidd.c's sanitize_field(). */
static void sanitize_field(char *s)
{
    if (!s) return;
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c < 0x20 || c == '"' || c == '|' || c == '\\')
            *s = '_';
    }
}

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

static void slog(const char *level, const char *fmt, ...)
{
    time_t t = time(NULL);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", localtime(&t));

    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[%s][SHREDDER/%s] ", ts, level);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

static void add_reason(char *buf, size_t bufsize, const char *reason, int points) {
    char entry[256];
    snprintf(entry, sizeof(entry), "{\"reason\":\"%s\",\"points\":-%d},", reason, points);
    size_t len = strlen(buf);
    if (len + strlen(entry) < bufsize - 1)
        strncat(buf, entry, bufsize - len - 1);
}

/* Bonus fix (see v2.3 changelog): each read now gets its own buffer.
 * Previously all three aliased the same `char buf[4096]`. */
static void check_filesystem_integrity(char *fs_json, size_t fs_size) {
    fs_json[0] = 0;
    char sysdm_buf[64], vendordm_buf[64], sysrw_buf[64];

    char *system_dm = probe_copy("SYSDM", sysdm_buf, sizeof(sysdm_buf));
    char *vendor_dm = probe_copy("VENDORDM", vendordm_buf, sizeof(vendordm_buf));
    char *system_rw = probe_copy("SYSRW", sysrw_buf, sizeof(sysrw_buf));

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

/* Standalone -- not batched, and deliberately so. Cached after first
 * successful parse (kernel_config_parsed); re-issuing this inside the
 * per-poll batch would pay for the ~570-byte regex command and its
 * response every 30s forever, for a value only the very first poll
 * actually needs. */
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
    char *vmstat = probe_copy("VMSTAT", buf, sizeof(buf));

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
    char *load = probe_copy("LOADAVG", buf, sizeof(buf));
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
    char *stat = probe_copy("BTIME", buf, sizeof(buf));
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
    char *nmi = probe_copy("NMI", buf, sizeof(buf));
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
    char *lsmod_out = probe_copy("LSMOD", lbuf, sizeof(lbuf));
    int permanent_count = 0;
    int sample_module_found = 0;
    if (lsmod_out && strlen(lsmod_out) > 0) {
        char *line = strtok(lsmod_out, "\n");
        if (line) line = strtok(NULL, "\n");
        while (line) {
            char name[64] = "";
            sscanf(line, "%63s", name);
            if (strlen(name) > 0) {
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
    char *uptime = probe_copy("UPTIME", buf, sizeof(buf));
    double uptime_sec = (uptime && strlen(uptime) > 0) ? atof(uptime) : -1.0;

    snprintf(json, json_size,
        "{\"kernel_sysctls_blocked\":true,\"uptime_sec\":%.2f,"
        "\"assessment\":\"proc sys kernel blocked by SELinux -- expected on hardened builds\"}",
        uptime_sec);
}

static void check_kernel_build(char *json, size_t json_size) {
    char buf[1024];
    char *ver = probe_copy("PROCVERSION", buf, sizeof(buf));
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
    char *tags = probe_copy("BUILDTAGS", buf, sizeof(buf));
    char tags_copy[64];
    strncpy(tags_copy, (tags && strlen(tags) > 0) ? tags : "unknown", sizeof(tags_copy) - 1);
    tags_copy[sizeof(tags_copy) - 1] = 0;

    char buf2[256];
    char *type = probe_copy("BUILDTYPE", buf2, sizeof(buf2));
    char type_copy[32];
    strncpy(type_copy, (type && strlen(type) > 0) ? type : "unknown", sizeof(type_copy) - 1);
    type_copy[sizeof(type_copy) - 1] = 0;

    char buf3[512];
    char *fp = probe_copy("BUILDFP", buf3, sizeof(buf3));
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
    char *adbd = probe_copy("ADBD", buf, sizeof(buf));
    char buf2[128];
    char *usbcfg = probe_copy("USBCFG", buf2, sizeof(buf2));

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
        slog("ERROR", "cannot write %s", RESULTS_FILE);
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
    slog("INFO", "JSON written: score=%d grade=%s drift_confirmed=%s",
        score, grade, confirmed_drift ? "YES" : "no");
}

static void poll_integrity(void) {
    int score = 100;
    char score_reasons[3072] = "";
    char raw_reads_json[1024] = "";
    char buf[8192];

    /* ── One combined bexec() round-trip (4 chunks) for this poll's raw
     *    reads. nmod's retry loop below stays standalone (conditional
     *    backoff doesn't fold into a single call). ── */
    load_probe_data();

    char su_buf[16];
    char *su_res = probe_copy("SUCHECK", su_buf, sizeof(su_buf));
    int su_found = contains(su_res, "yes");

    char magisk_buf[32];
    char *magisk_res = probe_copy("MAGISK", magisk_buf, sizeof(magisk_buf));
    int magisk = contains(magisk_res, "yes");
    char ksu_buf[32];
    char *ksu_res = probe_copy("KSU", ksu_buf, sizeof(ksu_buf));
    int kernelsu = contains(ksu_res, "yes");

    char vb_state_buf[128], vb_mode_buf[128];
    char *vb_state = probe_copy("VBSTATE", vb_state_buf, sizeof(vb_state_buf));
    char *vb_mode  = probe_copy("VBMODE", vb_mode_buf, sizeof(vb_mode_buf));
    int vb_ok = contains(vb_state, "green") || contains(vb_state, "yellow");
    if (!vb_ok) {
        add_reason(score_reasons, sizeof(score_reasons), "verified_boot_state not green/yellow", 10);
        score -= 10;
    }

    /* Folded into the batched MODULES chunk as of the MODCOUNT patch --
     * was a standalone rish_read() with a 3-retry/2s-backoff loop that
     * failed 3/3 fast under intermittent load while the batched MODULES
     * chunk (same poll cycle) succeeded. Root cause: per-call rish/binder
     * startup overhead amortized across a batch but not absorbed by a
     * single standalone call. */
    char modcount_buf[32];
    char *modcount = probe_copy("MODCOUNT", modcount_buf, sizeof(modcount_buf));
    int nmod = (modcount && strlen(modcount) > 0) ? atoi(modcount) : 0;
    if (nmod <= 0) {
        slog("ERROR", "FATAL: module count unreadable, skipping poll");
        return;
    }

    char suspicious_mod[64] = "";
    char susp_buf[256];
    char *susp = probe_copy("SUSPMOD", susp_buf, sizeof(susp_buf));
    if (susp && strlen(susp) > 0) strncpy(suspicious_mod, susp, sizeof(suspicious_mod) - 1);
    if (suspicious_mod[0]) {
        add_reason(score_reasons, sizeof(score_reasons), "suspicious module name matched frida/hook/inject/rootkit/backdoor pattern", 20);
        score -= 20;
    }

    char dfs_buf[32];
    char *dfs = probe_copy("DEBUGFS", dfs_buf, sizeof(dfs_buf));
    int debugfs = contains(dfs, "1");
    if (debugfs) {
        add_reason(score_reasons, sizeof(score_reasons), "debugfs mounted", 5);
        score -= 5;
    }

    char rp_buf[32];
    char *rp = probe_copy("ROOTPROCS", rp_buf, sizeof(rp_buf));
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
    char dmesg_buf[4096];
    char *dmesg_recent = probe_copy("DMESG", dmesg_buf, sizeof(dmesg_buf));
    if (dmesg_recent && strlen(dmesg_recent) > 0) {
        char *line = strtok(dmesg_recent, "\n");
        while (line) {
            sanitize_field(line);
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
        splinterd_emit("reboot_detected", "Kernel reboot detected via btime change");
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
        splinterd_emit("kernel_threat", threat_indicator);
        if (confirmed_drift && drift_delta > 0) {
            gaveld_emit(DAEMON_NAME, "NEW_KERNEL_MODULE", 0.0, new_mods);
            splinterd_emit("new_kernel_module", new_mods);
        }
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
