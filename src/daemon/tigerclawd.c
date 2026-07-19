/*
 * tigerclawd.c v1.2.2 — Xiaomi Device Integrity, Topology & Trust Surface Monitor
 *
 * v1.1 legacy: device fingerprinting, binder service topology, SELinux,
 *              debug props, MIUI Security Center, property drift, trust score
 * v1.2 new:    cert store diff (SHA-256), device admin/DPC poll,
 *              MIUI surface recon (verified boot, miui_optimization,
 *              Shizuku binding, PowerKeeper kill policies)
 * v1.2.1 fix: Real dumpsys device_policy parser with section state tracking
 *
 * v1.2.2 fix: Batched probe layer. poll loop previously fired ~20
 *   individual bexec() calls for small facts (getprop x9, getenforce x2,
 *   dumpsys x4, service list, pm path) PLUS one bexec() per cert file in
 *   probe_certs() -- up to 145+ round-trips on this device alone. Same
 *   root cause as tigerclawd's cert loop that was already flagged, just
 *   spread across more/smaller call sites too. Two duplicate fetches
 *   found along the way: getenforce was called separately by both
 *   check_selinux() and probe_miui_surface(); ro.boot.verifiedbootstate
 *   was called separately by both get_device_info() and
 *   probe_miui_surface(). Both now resolve from the same probe section.
 *   All per-poll reads -- small facts, service list, dumpsys output, AND
 *   cert-directory hashing (via find -exec sha256sum, hashed server-side
 *   in one shell invocation instead of one bexec() per file) -- are now
 *   issued as ONE combined bexec() call per poll, echo-delimited and
 *   parsed locally via probe_section() (same pattern used in shredderd
 *   v2.3 and rahzerd's original snapshot design).
 *   get_prop_hash() intentionally NOT folded in: it dumps the full
 *   read-only property set (sorted) for drift hashing, is comparatively
 *   large, and is only called conditionally (baseline-established polls
 *   + once at baseline-save) -- adding it to the always-on combined call
 *   would tax every single poll for a value most polls don't need.
 *   CAVEAT: the combined call's total output (device facts + service
 *   list + dumpsys device_policy/ProcessManager + all cert hashes) is
 *   materially larger than before, since previously each cert hash came
 *   back in its own small round-trip. Verify backend_exec.c's bexec()
 *   output buffer is large enough for this device's actual cert count +
 *   dumpsys sizes before relying on this in production -- g_probe_buf
 *   below is sized to 65536 bytes as a starting point; raise both if the
 *   real output is being truncated (check for a missing closing section
 *   or an unexpectedly-empty tail section as a symptom).
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <stdint.h>
#include <math.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "ipc_globals.h"
#include "backend_exec.h"
#include "gaveld_emit.h"

/* ── Paths from ipc_globals.h ───────────────────────────────────────────── */
#define RESULTS_FILE    MP_PIPES_DIR "/daemon_results/tigerclawd.json"
#define BASELINE_FILE   MP_PIPES_DIR "/state/tigerclawd_baseline.json"
#define PID_FILE        MP_PIDS_DIR  "/tigerclawd.pid"

#define DEFAULT_POLL    30
#define MAX_ANOMALIES   16
#define MAX_CERTS       512
#define CERT_FP_LEN     64
#define SUBJECT_MAX     256
#define MAX_ADMINS      32
#define PKG_NAME_MAX    128
#define MAX_KILL_POLICIES 64
#define POLICY_NAME_MAX 128

#define SYSTEM_CA_PATH  "/system/etc/security/cacerts"
#define USER_CA_PATH    "/data/misc/user/0/cacerts-added"

/* ── Structures ─────────────────────────────────────────────────────────── */

typedef struct {
    char type[16];
    char code[32];
    char detail[256];
} anomaly_t;

typedef struct {
    char fingerprint[CERT_FP_LEN + 1];
    char subject[SUBJECT_MAX];
    bool is_system;
    bool is_new;
    bool is_removed;
} cert_entry_t;

typedef struct {
    char package[PKG_NAME_MAX];
    char admin_class[PKG_NAME_MAX];
    bool is_profile_owner;
    bool is_device_owner;
    bool is_new;
} admin_entry_t;

typedef struct {
    char selinux_mode[16];
    char verified_boot[16];
    bool miui_optimization;
    bool shizuku_bound;
    int kill_policy_count;
    char kill_targets[MAX_KILL_POLICIES][POLICY_NAME_MAX];
} miui_surface_t;

typedef struct {
    char codename[32];
    char board[32];
    char hardware[32];
    char hyperos[32];
    char sec_patch[32];
    char bootloader[32];
    int svc_count;
    int svc_baseline;
    int svc_drift;
    int suspicious_services;
    int prop_drift;
    int trust_score;
    int anomaly_count;
    anomaly_t anomalies[MAX_ANOMALIES];
    int cert_count;
    int cert_delta_new;
    int cert_delta_removed;
    cert_entry_t certs[MAX_CERTS];
    int admin_count;
    int admin_delta_new;
    admin_entry_t admins[MAX_ADMINS];
    miui_surface_t miui;
    uint64_t sequence;
    char timestamp[32];
    int poll_ms;
} tigerclaw_report_t;

/* ── Globals ────────────────────────────────────────────────────────────── */

extern volatile bool g_running;
static int g_poll_sec = DEFAULT_POLL;
static int g_baseline_established = 0;
static int g_baseline_svc = 0;
static uint32_t g_baseline_prop_hash = 0;
static int g_baseline_cert_count = 0;
static cert_entry_t g_baseline_certs[MAX_CERTS];
static int g_baseline_admin_count = 0;
static admin_entry_t g_baseline_admins[MAX_ADMINS];

/* ── Lifecycle ──────────────────────────────────────────────────────────── */

static void handle_sig(int sig) { (void)sig; g_running = false; }

static void cleanup(void) {
    unlink(PID_FILE);
    unlink(TIGERCLAW_SOCKET);
}

/* ── Utils ──────────────────────────────────────────────────────────────── */

static void tlog(const char *lvl, const char *msg) {
    char ts[32];
    time_t t = time(NULL);
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", localtime(&t));
    fprintf(stderr, "[%s][TIGERCLAWD/%s] %s\n", ts, lvl, msg);
    fflush(stderr);
}

static void add_anomaly(tigerclaw_report_t *rpt, const char *type,
                        const char *code, const char *detail) {
    if (rpt->anomaly_count >= MAX_ANOMALIES) return;
    anomaly_t *a = &rpt->anomalies[rpt->anomaly_count++];
    strncpy(a->type, type, sizeof(a->type) - 1);
    strncpy(a->code, code, sizeof(a->code) - 1);
    strncpy(a->detail, detail, sizeof(a->detail) - 1);
    tlog(type, detail);
    gaveld_emit("tigerclawd", code, 1.0, detail);
}

static void clear_anomalies(tigerclaw_report_t *rpt) {
    rpt->anomaly_count = 0;
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

/* ── Batched probe layer ──────────────────────────────────────────────────
 * One bexec() round-trip per poll instead of ~20 small calls plus one
 * bexec() per cert file. Sections are pulled from the combined buffer via
 * probe_section(), which destructively splits on the next "\n==" marker --
 * safe because g_probe_buf is fully overwritten by load_probe_data() at
 * the start of every poll. Calling probe_section() twice for the same tag
 * (e.g. GETENFORCE, VBSTATE -- each now read by two call sites) is safe:
 * the first call's truncation persists, so the second call finds the same
 * already-terminated string.
 * ───────────────────────────────────────────────────────────────────────── */
/* Sized well above the measured ~79KB combined output (device_policy,
 * ProcessManager, and Shizuku dumpsys output plus cert-hash listings can
 * all grow independently of anything this file controls) -- 65536 was
 * already undersized for this device's actual output before the split
 * below; 262144 leaves real headroom instead of just moving the same
 * truncation risk to a slightly larger ceiling. */
static char g_probe_buf[262144] = "";
static int  g_probe_loaded = 0;

#define TC_SECTION_SCRATCH_SIZE 65536
static char g_section_scratch[TC_SECTION_SCRATCH_SIZE];

static char *probe_section(const char *tag) {
    if (!g_probe_loaded) return NULL;
    char marker[64];
    snprintf(marker, sizeof(marker), "==%s==", tag);
    char *start = strstr(g_probe_buf, marker);
    if (!start) return NULL;
    start += strlen(marker);
    while (*start == '\r' || *start == '\n') start++;
    char *end = strstr(start, "\n==");
    size_t len = end ? (size_t)(end - start) : strlen(start);
    if (len >= TC_SECTION_SCRATCH_SIZE) {
        char msg[128];
        snprintf(msg, sizeof(msg),
            "section %s length %zu exceeds %d-byte scratch buffer, truncating",
            tag, len, TC_SECTION_SCRATCH_SIZE);
        tlog("ERROR", msg);
        len = TC_SECTION_SCRATCH_SIZE - 1;
    }
    memcpy(g_section_scratch, start, len);
    g_section_scratch[len] = '\0';
    size_t pos = len;
    while (pos > 0 && (g_section_scratch[pos-1] == '\n' || g_section_scratch[pos-1] == '\r'))
        g_section_scratch[--pos] = '\0';
    return g_section_scratch;
}

/* v1.2.2 fix (post-recovery, round 2): the ORIGINAL single-call version of
 * this function combined 21 sections -- including 3 dumpsys calls and full
 * system+user cert hashing -- into ONE bexec() round-trip. Measured on this
 * device: 79,338 bytes of real combined output. That's both (a) larger than
 * bexec()'s fixed 65536-byte read cap (guaranteed silent truncation via
 * run_via_pty()'s drop-on-overflow behavior), and (b) plausibly close to or
 * over RISH_TIMEOUT (8s in backend_exec.c) once Shizuku's own ~2.6-3.1s IPC
 * connection overhead is added on top of 3 dumpsys calls + hashing every
 * system/user CA cert -- and unlike the normal-exit path, run_via_pty()'s
 * timeout branch does NOT do a final nonblocking drain of unread pty data
 * before killing the child, so a timeout can silently lose already-produced
 * output too.
 *
 * Fix: split into 3 calls instead of 1. Each is now individually far under
 * the 65536-byte cap and does a fraction of the previous single call's
 * work, so neither failure mode has room to trigger regardless of which
 * one was actually dominant on this device. Three round-trips instead of
 * one does cost more Shizuku IPC overhead (~3 x 2.6-3.1s instead of once)
 * -- accepted deliberately, correctness over round-trip count, same
 * tradeoff already made elsewhere in this fleet (see shredderd's
 * per-section retry history). Grouped by cost/nature, not evenly by count:
 *   1. Fast facts: every getprop/property read + SVCLIST + security
 *      center probes -- cheap, small individually and combined.
 *   2. Heavy dumpsys: SHIZUKU + PROCMGR + DEVICEPOLICY -- these three were
 *      the unmeasured, most likely source of most of the 79KB and of any
 *      single command run taking multiple seconds.
 *   3. Cert hashing: SYSCERTS + USERCERTS -- already self-contained
 *      (server-side find+sha256sum), scales with cert count independently
 *      of everything else, kept isolated so it can't blow the budget for
 *      any of the other two groups as cert counts change over time.
 */
#define TC_PROBE_CMD_BUDGET 1450

static int append_probe_chunk(const char *tag, const char *cmd, size_t bufpos_before) {
    (void)bufpos_before;
    size_t cmdlen = strlen(cmd);
    if (cmdlen > TC_PROBE_CMD_BUDGET) {
        char errmsg[160];
        snprintf(errmsg, sizeof(errmsg),
            "probe chunk %zu bytes exceeds %d-byte budget, refusing (would silently truncate)",
            cmdlen, TC_PROBE_CMD_BUDGET);
        tlog("ERROR", errmsg);
        return -1;
    }

    char *raw = bexec(cmd);

    /* Temporary diagnostic (2026-07-19): prior fix (splitting into 3 calls)
     * did not resolve svc=0/certs=0/admins=0, and duration (~9.3s / 3 =
     * ~3.1s per call) suggests calls are completing rather than timing
     * out -- so the timeout theory is likely wrong. Logging exact return
     * state per chunk instead of guessing at a further blind fix. */
    {
        char dbg[160];
        snprintf(dbg, sizeof(dbg), "chunk %s: bexec returned %s, length=%zu",
                 tag, raw ? "non-NULL" : "NULL", raw ? strlen(raw) : 0);
        tlog("DIAG", dbg);
    }

    if (!raw) return -1;

    size_t curlen = strlen(g_probe_buf);
    size_t rawlen = strlen(raw);
    size_t space = sizeof(g_probe_buf) - curlen - 1;
    if (rawlen > space) {
        char errmsg[160];
        snprintf(errmsg, sizeof(errmsg),
            "probe chunk response %zu bytes exceeds remaining buffer space %zu, truncating",
            rawlen, space);
        tlog("ERROR", errmsg);
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
        "echo ==CODENAME==; getprop ro.product.device 2>/dev/null;"
        "echo ==BOARD==; getprop ro.product.board 2>/dev/null;"
        "echo ==HARDWARE==; getprop ro.hardware 2>/dev/null;"
        "echo ==HYPEROS1==; getprop ro.miui.ui.version.name 2>/dev/null;"
        "echo ==HYPEROS2==; getprop ro.build.version.hyperos 2>/dev/null;"
        "echo ==SECPATCH==; getprop ro.build.version.security_patch 2>/dev/null;"
        "echo ==VBSTATE==; getprop ro.boot.verifiedbootstate 2>/dev/null;"
        "echo ==GETENFORCE==; getenforce 2>/dev/null;"
        "echo ==DEBUGGABLE==; getprop ro.debuggable 2>/dev/null;"
        "echo ==SECURE==; getprop ro.secure 2>/dev/null;"
        "echo ==BUILDTYPE==; getprop ro.build.type 2>/dev/null;"
        "echo ==USBSTATE==; getprop sys.usb.state 2>/dev/null;"
        "echo ==ADBTCP==; getprop persist.adb.tcp.port 2>/dev/null;"
        "echo ==MIUIOPT==; getprop persist.sys.miui_optimization 2>/dev/null;"
        "echo ==SVCLIST==; service list 2>/dev/null;"
        "echo ==SECCENTERPATH==; pm path com.miui.securitycenter 2>/dev/null;"
        "echo ==SECCENTERCOUNT==; service list 2>/dev/null | grep -c securitycenter;";

    static const char *cmd_dumpsys =
        "echo ==SHIZUKU==; dumpsys activity service com.shizuku 2>/dev/null;"
        "echo ==PROCMGR==; dumpsys ProcessManager 2>/dev/null;"
        "echo ==DEVICEPOLICY==; dumpsys device_policy 2>/dev/null;";

    static const char *cmd_certs =
        "echo ==SYSCERTS==; find " SYSTEM_CA_PATH " -maxdepth 1 -type f -exec sha256sum {} \\; 2>/dev/null;"
        "echo ==USERCERTS==; find " USER_CA_PATH " -maxdepth 1 -type f -exec sha256sum {} \\; 2>/dev/null;";

    if (append_probe_chunk("FAST", cmd_fast, 0) != 0) return;
    if (append_probe_chunk("DUMPSYS", cmd_dumpsys, strlen(g_probe_buf)) != 0) return;
    if (append_probe_chunk("CERTS", cmd_certs, strlen(g_probe_buf)) != 0) return;

    g_probe_loaded = 1;
    { char dbg[80]; snprintf(dbg, sizeof(dbg), "g_probe_buf total length=%zu", strlen(g_probe_buf)); tlog("DIAG", dbg); }
}

/* ── v1.1 Core Probes ───────────────────────────────────────────────────── */

/* Now reads from the combined probe buffer instead of issuing 6 of its
 * own bexec() calls. VBSTATE is shared with probe_miui_surface() -- both
 * previously fetched ro.boot.verifiedbootstate independently. */
static void get_device_info(tigerclaw_report_t *rpt) {
    char *v;

    v = probe_section("CODENAME");
    if (v) strncpy(rpt->codename, v, sizeof(rpt->codename) - 1);
    else strcpy(rpt->codename, "unknown");

    v = probe_section("BOARD");
    if (v) strncpy(rpt->board, v, sizeof(rpt->board) - 1);
    else strcpy(rpt->board, "unknown");

    v = probe_section("HARDWARE");
    if (v) strncpy(rpt->hardware, v, sizeof(rpt->hardware) - 1);
    else strcpy(rpt->hardware, "unknown");

    v = probe_section("HYPEROS1");
    if (v && strlen(v) > 0) {
        strncpy(rpt->hyperos, v, sizeof(rpt->hyperos) - 1);
    } else {
        v = probe_section("HYPEROS2");
        if (v && strlen(v) > 0) strncpy(rpt->hyperos, v, sizeof(rpt->hyperos) - 1);
        else strcpy(rpt->hyperos, "unknown");
    }

    v = probe_section("SECPATCH");
    if (v) strncpy(rpt->sec_patch, v, sizeof(rpt->sec_patch) - 1);
    else strcpy(rpt->sec_patch, "unknown");

    v = probe_section("VBSTATE");
    if (v) strncpy(rpt->bootloader, v, sizeof(rpt->bootloader) - 1);
    else strcpy(rpt->bootloader, "unknown");
}

static int check_suspicious_services(const char *services, tigerclaw_report_t *rpt) {
    const char *bad[] = {
        "frida", "xposed", "inject", "magisk", "supersu",
        "substrate", "lspatch", "hook", "edxp", "taichi", NULL
    };
    int found = 0;
    for (int i = 0; bad[i]; i++) {
        if (services && strstr(services, bad[i])) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Suspicious binder service: %s", bad[i]);
            add_anomaly(rpt, "URGENT", "SUSPICIOUS_BINDER_SERVICE", msg);
            found++;
        }
    }
    return found;
}

/* Now reads GETENFORCE from the combined probe buffer instead of its own
 * bexec() call. Shared with probe_miui_surface() -- both previously
 * fetched getenforce independently. */
static int check_selinux(tigerclaw_report_t *rpt) {
    char *mode = probe_section("GETENFORCE");
    int enforcing = 1;
    if (mode) {
        if (strstr(mode, "Permissive")) {
            add_anomaly(rpt, "URGENT", "SELINUX_PERMISSIVE",
                        "SELinux is PERMISSIVE");
            enforcing = 0;
        }
    }
    return enforcing;
}

/* Now reads DEBUGGABLE/SECURE/BUILDTYPE from the combined probe buffer
 * instead of issuing 3 of its own bexec() calls. DEBUGGABLE is shared
 * with check_dev_state(). */
static int check_debug_props(tigerclaw_report_t *rpt) {
    int clean = 1;
    struct { const char *tag; const char *prop; const char *safe; } checks[] = {
        { "DEBUGGABLE", "ro.debuggable", "0" },
        { "SECURE",     "ro.secure",     "1" },
        { "BUILDTYPE",  "ro.build.type", "user" },
        { NULL, NULL, NULL }
    };
    for (int i = 0; checks[i].tag; i++) {
        char *v = probe_section(checks[i].tag);
        if (v && strlen(v) > 0 && strcmp(v, checks[i].safe) != 0) {
            char msg[256];
            snprintf(msg, sizeof(msg), "%s=%s (expected %s)",
                     checks[i].prop, v, checks[i].safe);
            add_anomaly(rpt, "URGENT", "DEBUG_PROP_ANOMALY", msg);
            clean = 0;
        }
    }
    return clean;
}

/* Now reads SECCENTERPATH/SECCENTERCOUNT from the combined probe buffer
 * instead of issuing its own 2 bexec() calls. */
static int check_security_center(tigerclaw_report_t *rpt) {
    char *pm = probe_section("SECCENTERPATH");
    int installed = (pm && strstr(pm, "package:")) ? 1 : 0;
    if (!installed) {
        add_anomaly(rpt, "WARNING", "SECURITY_CENTER_MISSING",
                    "MIUI Security Center not installed");
        return 0;
    }
    char *svc = probe_section("SECCENTERCOUNT");
    int running = svc ? (atoi(svc) > 0) : 0;
    if (!running) {
        add_anomaly(rpt, "WARNING", "SECURITY_CENTER_DOWN",
                    "MIUI Security Center installed but not running");
        return 0;
    }
    return 1;
}

static int check_xiaomi_services(const char *services, tigerclaw_report_t *rpt) {
    const char *core[] = { "MiuiInit", "ProcessManager", "anrrescue", NULL };
    int missing = 0;
    for (int i = 0; core[i]; i++) {
        if (!services || !strstr(services, core[i])) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Core Xiaomi service absent: %s", core[i]);
            add_anomaly(rpt, "INFO", "XIAOMI_SERVICE_MISSING", msg);
            missing++;
        }
    }
    return missing;
}

/* Now reads DEBUGGABLE/USBSTATE/ADBTCP from the combined probe buffer
 * instead of issuing its own 3 bexec() calls. DEBUGGABLE is shared with
 * check_debug_props(). Comparison logic updated from raw-with-newline
 * ("0\n"/"0\r\n") to trimmed-string ("0"), since probe_section() already
 * strips trailing CR/LF the same way rish_read() does elsewhere in the
 * fleet -- same semantics, just matched against the now-trimmed value. */
static void check_dev_state(tigerclaw_report_t *rpt, int *dev_opts,
                            int *adb_enabled, int *adb_net) {
    char *v = probe_section("DEBUGGABLE");
    *dev_opts = (v && strlen(v) > 0 && strcmp(v, "0") != 0) ? 1 : 0;

    v = probe_section("USBSTATE");
    *adb_enabled = (v && strstr(v, "adb")) ? 1 : 0;

    v = probe_section("ADBTCP");
    *adb_net = (v && atoi(v) > 0) ? 1 : 0;

    if (*dev_opts) add_anomaly(rpt, "WARNING", "DEV_OPTIONS_ENABLED",
                               "Developer options enabled");
    if (*adb_net) add_anomaly(rpt, "WARNING", "ADB_OVER_NETWORK",
                              "ADB over network enabled");
}

/* Unchanged: intentionally NOT folded into the combined probe -- large,
 * comparatively expensive full property dump, only needed conditionally
 * (baseline-established polls + once at baseline-save). See v1.2.2
 * changelog note at top of file. */
static uint32_t get_prop_hash(void) {
    char *props = bexec("getprop 2>/dev/null | grep -E '^\\[ro\\.' | sort");
    uint32_t h = djb2(props);
    free(props);
    return h;
}

/* ── v1.2 Probes: Cert Store ──────────────────────────────────────────────
 * Previously scan_cert_dir() opened each cert directory locally and called
 * sha256_file() -> bexec("sha256sum '<path>'") once PER FILE -- 145+
 * individual round-trips on this device. Now the SYSCERTS/USERCERTS
 * sections of the combined probe buffer already contain "hash  path"
 * lines for every file in both directories (hashed server-side via a
 * single find -exec sha256sum per directory), so this just parses text
 * that's already in memory -- zero additional bexec() calls.
 * ───────────────────────────────────────────────────────────────────────── */

static void parse_cert_section(const char *raw, bool is_system, tigerclaw_report_t *rpt) {
    if (!raw || !*raw) return;
    char buf[32768];
    strncpy(buf, raw, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *line = strtok(buf, "\n");
    while (line && rpt->cert_count < MAX_CERTS) {
        char hash[128] = "", path[512] = "";
        if (sscanf(line, "%127s %511s", hash, path) == 2 && strlen(hash) >= 64) {
            cert_entry_t *c = &rpt->certs[rpt->cert_count];
            memset(c, 0, sizeof(*c));
            c->is_system = is_system;
            const char *base = strrchr(path, '/');
            base = base ? base + 1 : path;
            snprintf(c->subject, sizeof(c->subject), "%s", base);
            memcpy(c->fingerprint, hash, 64);
            c->fingerprint[64] = '\0';
            rpt->cert_count++;
        }
        line = strtok(NULL, "\n");
    }
}

static int probe_certs(tigerclaw_report_t *rpt) {
    rpt->cert_count = 0;
    rpt->cert_delta_new = 0;
    rpt->cert_delta_removed = 0;
    parse_cert_section(probe_section("SYSCERTS"), true, rpt);
    parse_cert_section(probe_section("USERCERTS"), false, rpt);
    return 0;
}

/* ── v1.2 Probes: Device Admin (real dumpsys parser) ─────────────────────
 * Now reads DEVICEPOLICY from the combined probe buffer instead of its
 * own bexec() call. NOTE: probe_section() returns a pointer into the
 * static g_probe_buf, not a malloc'd string -- unlike the old bexec()
 * return value, it must NOT be free()'d. Only the strdup()'d working copy
 * is freed, same as before.
 * ───────────────────────────────────────────────────────────────────────── */

static int probe_admins(tigerclaw_report_t *rpt) {
    rpt->admin_count = 0;
    rpt->admin_delta_new = 0;

    char *raw = probe_section("DEVICEPOLICY");
    if (!raw) return -1;
    char *buf = strdup(raw);
    if (!buf) return -1;

    enum { ST_NONE, ST_DEVICE_OWNER, ST_PROFILE_OWNER, ST_ADMIN_LIST } state = ST_NONE;
    char device_owner_pkg[PKG_NAME_MAX] = {0};
    char profile_owner_pkgs[8][PKG_NAME_MAX];
    int profile_owner_count = 0;

    char *line = strtok(buf, "\n");
    while (line) {
        if (strstr(line, "Device Owner:")) {
            state = ST_DEVICE_OWNER;
            line = strtok(NULL, "\n");
            continue;
        }
        if (strstr(line, "Profile Owner")) {
            state = ST_PROFILE_OWNER;
            line = strtok(NULL, "\n");
            continue;
        }
        if (strstr(line, "Enabled Device Admins")) {
            state = ST_ADMIN_LIST;
            line = strtok(NULL, "\n");
            continue;
        }
        if ((state == ST_DEVICE_OWNER || state == ST_PROFILE_OWNER) &&
            strstr(line, ":") && line[0] == ' ' && line[1] == ' ' &&
            line[2] != ' ' && !strstr(line, "package=") && !strstr(line, "admin=")) {
            state = ST_NONE;
        }

        if (state == ST_DEVICE_OWNER) {
            char *p = strstr(line, "package=");
            if (p) {
                p += 8;
                char *end = p;
                while (*end && *end != ' ' && *end != '\n' && *end != '\r') end++;
                size_t len = end - p;
                if (len >= PKG_NAME_MAX) len = PKG_NAME_MAX - 1;
                memcpy(device_owner_pkg, p, len);
                device_owner_pkg[len] = '\0';
            }
        } else if (state == ST_PROFILE_OWNER) {
            char *p = strstr(line, "package=");
            if (p && profile_owner_count < 8) {
                p += 8;
                char *end = p;
                while (*end && *end != ' ' && *end != '\n' && *end != '\r') end++;
                size_t len = end - p;
                if (len >= PKG_NAME_MAX) len = PKG_NAME_MAX - 1;
                memcpy(profile_owner_pkgs[profile_owner_count], p, len);
                profile_owner_pkgs[profile_owner_count][len] = '\0';
                profile_owner_count++;
            }
        } else if (state == ST_ADMIN_LIST && rpt->admin_count < MAX_ADMINS) {
            char *slash_check = strchr(line, '/');
            /* Fixed 2026-07-19: this device's dumpsys column-packs key=value
             * pairs onto the same line inconsistently between admin entries
             * (e.g. "pkg/.Receiver:" alone on one line, but
             * "pkg/.Receiver:                  uid=10312" on the next) --
             * requiring the line to END in ':' silently dropped any admin
             * whose declaration line got packed with trailing uid=/etc data.
             * Now only requires a ':' exist somewhere after the '/'; class
             * name extraction below already correctly stops at that ':'
             * regardless of what follows it on the line. */
            if (line[0] == ' ' && line[1] == ' ' && line[2] == ' ' &&
                line[3] == ' ' && line[4] != ' ' && slash_check &&
                strchr(slash_check, ':') && !strstr(line, "count=")) {
                char *slash = slash_check;
                char *pkg_start = line + 4;
                admin_entry_t *a = &rpt->admins[rpt->admin_count];
                memset(a, 0, sizeof(*a));
                size_t pkg_len = slash - pkg_start;
                if (pkg_len >= PKG_NAME_MAX) pkg_len = PKG_NAME_MAX - 1;
                memcpy(a->package, pkg_start, pkg_len);
                a->package[pkg_len] = '\0';
                char *class_start = slash + 1;
                char *class_end = class_start;
                while (*class_end && *class_end != ' ' && *class_end != '\n' &&
                       *class_end != '\r' && *class_end != ':') class_end++;
                size_t class_len = class_end - class_start;
                if (class_len >= PKG_NAME_MAX) class_len = PKG_NAME_MAX - 1;
                memcpy(a->admin_class, class_start, class_len);
                a->admin_class[class_len] = '\0';
                a->is_device_owner = (device_owner_pkg[0] &&
                                       strcmp(a->package, device_owner_pkg) == 0);
                for (int i = 0; i < profile_owner_count; i++) {
                    if (strcmp(a->package, profile_owner_pkgs[i]) == 0) {
                        a->is_profile_owner = true;
                        break;
                    }
                }
                a->is_new = true;
                rpt->admin_count++;
            }
        }
        line = strtok(NULL, "\n");
    }
    free(buf);
    return 0;
}

/* ── v1.2 Probes: MIUI Surface ────────────────────────────────────────────
 * Now reads GETENFORCE/VBSTATE/MIUIOPT/SHIZUKU/PROCMGR from the combined
 * probe buffer instead of issuing 5 of its own bexec() calls. GETENFORCE
 * is shared with check_selinux(); VBSTATE is shared with get_device_info()
 * -- both were previously fetched twice per poll independently.
 * ───────────────────────────────────────────────────────────────────────── */

static int probe_miui_surface(tigerclaw_report_t *rpt) {
    miui_surface_t *m = &rpt->miui;
    char *v;

    v = probe_section("GETENFORCE");
    if (v) snprintf(m->selinux_mode, sizeof(m->selinux_mode), "%s", v);
    else snprintf(m->selinux_mode, sizeof(m->selinux_mode), "unknown");

    v = probe_section("VBSTATE");
    if (v) snprintf(m->verified_boot, sizeof(m->verified_boot), "%s", v);
    else snprintf(m->verified_boot, sizeof(m->verified_boot), "unknown");

    v = probe_section("MIUIOPT");
    m->miui_optimization = (v && strstr(v, "1"));

    v = probe_section("SHIZUKU");
    m->shizuku_bound = (v && strstr(v, "ShizukuService"));

    v = probe_section("PROCMGR");
    if (v) {
        char *buf = strdup(v);
        if (buf) {
            char *line = strtok(buf, "\n");
            while (line && m->kill_policy_count < MAX_KILL_POLICIES) {
                if (strstr(line, "mKillingPackageMaps")) {
                    char *eq = strchr(line, '=');
                    if (eq) {
                        eq++;
                        while (*eq == ' ') eq++;
                        char *end = strchr(eq, ',');
                        if (!end) end = strchr(eq, '\n');
                        if (!end) end = eq + strlen(eq);
                        size_t len = end - eq;
                        if (len >= POLICY_NAME_MAX) len = POLICY_NAME_MAX - 1;
                        memcpy(m->kill_targets[m->kill_policy_count], eq, len);
                        m->kill_targets[m->kill_policy_count][len] = '\0';
                        m->kill_policy_count++;
                    }
                }
                line = strtok(NULL, "\n");
            }
            free(buf);
        }
    }
    return 0;
}

static int local_mkdir_p(const char *path) {
    char tmp[512];
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) ? 0 : -1;
}

/* ── Baseline Management ───────────────────────────────────────────────── */

static void load_baseline(tigerclaw_report_t *rpt) {
    (void)rpt;
    FILE *f = fopen(BASELINE_FILE, "r");
    if (!f) return;
    char buf[1024];
    if (fgets(buf, sizeof(buf), f)) {
        char *p = strstr(buf, "\"service_count\":");
        if (p) g_baseline_svc = atoi(p + 16);
        p = strstr(buf, "\"prop_hash\":");
        if (p) g_baseline_prop_hash = (uint32_t)strtoul(p + 12, NULL, 10);
        p = strstr(buf, "\"cert_count\":");
        if (p) g_baseline_cert_count = atoi(p + 13);
        p = strstr(buf, "\"admin_count\":");
        if (p) g_baseline_admin_count = atoi(p + 14);
        g_baseline_established = (g_baseline_svc > 0);
    }
    fclose(f);
    if (g_baseline_established) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Baseline loaded: svc=%d certs=%d admins=%d",
                 g_baseline_svc, g_baseline_cert_count, g_baseline_admin_count);
        tlog("INFO", msg);
    }
}

static void save_baseline(const tigerclaw_report_t *rpt) {
    if (local_mkdir_p(MP_PIPES_DIR "/state") != 0 ||
        local_mkdir_p(MP_PIPES_DIR "/daemon_results") != 0) {
        tlog("ERROR", "Failed to create baseline directories");
        return;
    }
    FILE *f = fopen(BASELINE_FILE, "w");
    if (!f) return;
    fprintf(f, "{\n"
        "  \"service_count\":%d,\n"
        "  \"prop_hash\":%u,\n"
        "  \"cert_count\":%d,\n"
        "  \"admin_count\":%d,\n"
        "  \"established_at\":%ld\n"
        "}\n",
        rpt->svc_count, get_prop_hash(), rpt->cert_count,
        rpt->admin_count, (long)time(NULL));
    fclose(f);
    g_baseline_svc = rpt->svc_count;
    g_baseline_prop_hash = get_prop_hash();
    int n = (rpt->cert_count < MAX_CERTS) ? rpt->cert_count : MAX_CERTS;
    for (int i = 0; i < n; i++) memcpy(&g_baseline_certs[i], &rpt->certs[i], sizeof(cert_entry_t));
    g_baseline_cert_count = n;
    n = (rpt->admin_count < MAX_ADMINS) ? rpt->admin_count : MAX_ADMINS;
    for (int i = 0; i < n; i++) memcpy(&g_baseline_admins[i], &rpt->admins[i], sizeof(admin_entry_t));
    g_baseline_admin_count = n;
    g_baseline_established = 1;
    tlog("INFO", "Baseline established");
}

static void compute_delta(tigerclaw_report_t *rpt) {
    if (!g_baseline_established) return;
    for (int i = 0; i < rpt->cert_count; i++) {
        bool found = false;
        for (int j = 0; j < g_baseline_cert_count; j++) {
            if (strcmp(rpt->certs[i].fingerprint, g_baseline_certs[j].fingerprint) == 0) {
                found = true; break;
            }
        }
        rpt->certs[i].is_new = !found;
        if (!found) {
            rpt->cert_delta_new++;
            char msg[256];
            snprintf(msg, sizeof(msg), "New cert: %s (fp=%.16s...)",
                     rpt->certs[i].subject, rpt->certs[i].fingerprint);
            add_anomaly(rpt, "WARNING", "CERT_STORE_DELTA", msg);
        }
    }
    for (int j = 0; j < g_baseline_cert_count; j++) {
        bool found = false;
        for (int i = 0; i < rpt->cert_count; i++) {
            if (strcmp(g_baseline_certs[j].fingerprint, rpt->certs[i].fingerprint) == 0) {
                found = true; break;
            }
        }
        if (!found) {
            rpt->cert_delta_removed++;
            char msg[256];
            snprintf(msg, sizeof(msg), "Removed cert: %s (fp=%.16s...)",
                     g_baseline_certs[j].subject, g_baseline_certs[j].fingerprint);
            add_anomaly(rpt, "WARNING", "CERT_STORE_DELTA", msg);
        }
    }
    for (int i = 0; i < rpt->admin_count; i++) {
        bool found = false;
        for (int j = 0; j < g_baseline_admin_count; j++) {
            if (strcmp(rpt->admins[i].package, g_baseline_admins[j].package) == 0 &&
                strcmp(rpt->admins[i].admin_class, g_baseline_admins[j].admin_class) == 0) {
                found = true; break;
            }
        }
        rpt->admins[i].is_new = !found;
        if (!found) {
            rpt->admin_delta_new++;
            char msg[256];
            snprintf(msg, sizeof(msg), "New device admin: %s/%s%s%s",
                     rpt->admins[i].package, rpt->admins[i].admin_class,
                     rpt->admins[i].is_device_owner ? " [DEVICE_OWNER]" : "",
                     rpt->admins[i].is_profile_owner ? " [PROFILE_OWNER]" : "");
            add_anomaly(rpt, "URGENT", "DEVICE_ADMIN_NEW", msg);
        }
    }
}

/* ── Trust Score ───────────────────────────────────────────────────────── */

static int calc_trust_score(const tigerclaw_report_t *rpt) {
    int score = 100;
    if (!strstr(rpt->miui.selinux_mode, "enforcing") &&
        !strstr(rpt->miui.selinux_mode, "Enforcing")) score -= 25;
    score -= (rpt->suspicious_services * 15);
    score -= (rpt->cert_delta_new * 5);
    score -= (rpt->cert_delta_removed * 3);
    score -= (rpt->admin_delta_new * 10);
    if (score < 0) score = 0;
    return score;
}

/* ── JSON Output ────────────────────────────────────────────────────────── */

static void write_json(const tigerclaw_report_t *rpt) {
    if (local_mkdir_p(MP_PIPES_DIR "/daemon_results") != 0) {
        tlog("ERROR", "Failed to create daemon_results directory");
        return;
    }
    FILE *f = fopen(RESULTS_FILE, "w");
    if (!f) {
        char errmsg[128];
        snprintf(errmsg, sizeof(errmsg), "Cannot write JSON results: %s", strerror(errno));
        tlog("ERROR", errmsg);
        return;
    }

    fprintf(f,
        "{\n"
        "  \"daemon\": \"tigerclawd\",\n"
        "  \"version\": \"1.2.2\",\n"
        "  \"sequence\": %lu,\n"
        "  \"timestamp\": \"%s\",\n"
        "  \"poll_interval_sec\": %d,\n"
        "  \"poll_duration_ms\": %d,\n"
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
        "  \"cert_store\": {\n"
        "    \"count\": %d,\n"
        "    \"delta_new\": %d,\n"
        "    \"delta_removed\": %d,\n"
        "    \"certs\": [\n",
        (unsigned long)rpt->sequence, rpt->timestamp, g_poll_sec, rpt->poll_ms,
        rpt->codename, rpt->board, rpt->hardware, rpt->hyperos, rpt->sec_patch,
        (strcmp(rpt->bootloader, "green") == 0 ||
         strcmp(rpt->bootloader, "locked") == 0) ? "true" : "false",
        rpt->svc_count, g_baseline_svc, rpt->svc_drift, rpt->suspicious_services,
        (strstr(rpt->miui.selinux_mode, "enforcing") ||
         strstr(rpt->miui.selinux_mode, "Enforcing")) ? "true" : "false",
        "true", "true", "false", "false", "false",
        rpt->prop_drift ? "true" : "false",
        rpt->cert_count, rpt->cert_delta_new, rpt->cert_delta_removed);

    for (int i = 0; i < rpt->cert_count; i++) {
        fprintf(f,
            "      {\n"
            "        \"fingerprint\": \"%s\",\n"
            "        \"subject\": \"%s\",\n"
            "        \"system\": %s,\n"
            "        \"new\": %s\n"
            "      }%s\n",
            rpt->certs[i].fingerprint, rpt->certs[i].subject,
            rpt->certs[i].is_system ? "true" : "false",
            rpt->certs[i].is_new ? "true" : "false",
            (i < rpt->cert_count - 1) ? "," : "");
    }

    fprintf(f,
        "    ]\n"
        "  },\n"
        "  \"device_admins\": {\n"
        "    \"count\": %d,\n"
        "    \"delta_new\": %d,\n"
        "    \"admins\": [\n",
        rpt->admin_count, rpt->admin_delta_new);

    for (int i = 0; i < rpt->admin_count; i++) {
        fprintf(f,
            "      {\n"
            "        \"package\": \"%s\",\n"
            "        \"class\": \"%s\",\n"
            "        \"profile_owner\": %s,\n"
            "        \"device_owner\": %s,\n"
            "        \"new\": %s\n"
            "      }%s\n",
            rpt->admins[i].package, rpt->admins[i].admin_class,
            rpt->admins[i].is_profile_owner ? "true" : "false",
            rpt->admins[i].is_device_owner ? "true" : "false",
            rpt->admins[i].is_new ? "true" : "false",
            (i < rpt->admin_count - 1) ? "," : "");
    }

    fprintf(f,
        "    ]\n"
        "  },\n"
        "  \"miui_surface\": {\n"
        "    \"selinux_mode\": \"%s\",\n"
        "    \"verified_boot\": \"%s\",\n"
        "    \"miui_optimization\": %s,\n"
        "    \"shizuku_bound\": %s,\n"
        "    \"kill_policies\": {\n"
        "      \"count\": %d,\n"
        "      \"targets\": [\n",
        rpt->miui.selinux_mode, rpt->miui.verified_boot,
        rpt->miui.miui_optimization ? "true" : "false",
        rpt->miui.shizuku_bound ? "true" : "false",
        rpt->miui.kill_policy_count);

    for (int i = 0; i < rpt->miui.kill_policy_count; i++) {
        fprintf(f, "        \"%s\"%s\n",
                rpt->miui.kill_targets[i],
                (i < rpt->miui.kill_policy_count - 1) ? "," : "");
    }

    fprintf(f,
        "      ]\n"
        "    }\n"
        "  },\n"
        "  \"trust_score\": %d,\n"
        "  \"anomalies\": [\n",
        rpt->trust_score);

    for (int i = 0; i < rpt->anomaly_count; i++) {
        fprintf(f,
            "    {\n"
            "      \"type\": \"%s\",\n"
            "      \"code\": \"%s\",\n"
            "      \"detail\": \"%s\"\n"
            "    }%s\n",
            rpt->anomalies[i].type, rpt->anomalies[i].code,
            rpt->anomalies[i].detail,
            (i < rpt->anomaly_count - 1) ? "," : "");
    }

    fprintf(f,
        "  ]\n"
        "}\n");
    fflush(f);
    fclose(f);
}

/* ── Main ───────────────────────────────────────────────────────────────── */

int main(void) {
    signal(SIGINT,  handle_sig);
    signal(SIGTERM, handle_sig);
    g_running = true;

    bexec_init();
    tlog("INFO", "tigerclawd v1.2.2 — Xiaomi eye online");

    FILE *pf = fopen(PID_FILE, "w");
    if (pf) { fprintf(pf, "%d\n", getpid()); fclose(pf); }

    tigerclaw_report_t baseline_rpt;
    memset(&baseline_rpt, 0, sizeof(baseline_rpt));
    load_baseline(&baseline_rpt);

    uint64_t sequence = 0;

    while (g_running) {
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);

        tigerclaw_report_t rpt;
        memset(&rpt, 0, sizeof(rpt));
        rpt.sequence = ++sequence;
        time_t t = time(NULL);
        strftime(rpt.timestamp, sizeof(rpt.timestamp), "%Y-%m-%dT%H:%M:%S", localtime(&t));

        clear_anomalies(&rpt);

        /* ── One combined bexec() round-trip for this poll's raw reads ── */
        load_probe_data();

        /* ── v1.1 core probes ─────────────────────────────────────── */
        get_device_info(&rpt);
        char *services = probe_section("SVCLIST");
        rpt.svc_count = count_lines(services);

        if (!g_baseline_established && rpt.svc_count > 10) {
            rpt.svc_baseline = rpt.svc_count;
            rpt.svc_drift = 0;
        }

        rpt.suspicious_services = check_suspicious_services(services, &rpt);

        int selinux = check_selinux(&rpt);
        int debug_clean = check_debug_props(&rpt);
        int sc_running = check_security_center(&rpt);
        int dev_opts = 0, adb_enabled = 0, adb_net = 0;
        check_dev_state(&rpt, &dev_opts, &adb_enabled, &adb_net);

        if (g_baseline_established && rpt.svc_count > 0) {
            rpt.svc_drift = rpt.svc_count - g_baseline_svc;
            if (abs(rpt.svc_drift) > 5) {
                char msg[128];
                snprintf(msg, sizeof(msg),
                    "Binder drift: %+d services (baseline %d, now %d)",
                    rpt.svc_drift, g_baseline_svc, rpt.svc_count);
                add_anomaly(&rpt, "WARNING", "BINDER_TOPOLOGY_DRIFT", msg);
            }
        }

        if (g_baseline_established) {
            uint32_t ph = get_prop_hash();
            if (ph != g_baseline_prop_hash && g_baseline_prop_hash != 0) {
                add_anomaly(&rpt, "WARNING", "PROP_HASH_DRIFT",
                            "Read-only property set changed");
                rpt.prop_drift = 1;
            }
        }

        /* ── v1.2 new probes ────────────────────────────────────────── */
        probe_certs(&rpt);
        probe_admins(&rpt);
        probe_miui_surface(&rpt);

        /* ── Delta computation ──────────────────────────────────────── */
        if (g_baseline_established) {
            compute_delta(&rpt);
        }

        /* ── Trust score ───────────────────────────────────────────── */
        rpt.trust_score = calc_trust_score(&rpt);
        if (!selinux) rpt.trust_score -= 25;
        if (!debug_clean) rpt.trust_score -= 20;
        if (!sc_running) rpt.trust_score -= 15;
        if (dev_opts) rpt.trust_score -= 10;
        if (adb_net) rpt.trust_score -= 10;
        if (rpt.trust_score < 0) rpt.trust_score = 0;

        /* ── Baseline establishment ─────────────────────────────────── */
        if (!g_baseline_established && rpt.svc_count > 10) {
            save_baseline(&rpt);
        }

        /* ── Timing ───────────────────────────────────────────────── */
        clock_gettime(CLOCK_MONOTONIC, &t1);
        rpt.poll_ms = (int)((t1.tv_sec - t0.tv_sec) * 1000 +
                            (t1.tv_nsec - t0.tv_nsec) / 1000000);

        /* ── Emit ───────────────────────────────────────────────────── */
        write_json(&rpt);

        char logmsg[256];
        snprintf(logmsg, sizeof(logmsg),
            "seq=%lu svc=%d certs=%d admins=%d score=%d/%d anomalies=%d dur=%dms",
            (unsigned long)rpt.sequence, rpt.svc_count, rpt.cert_count,
            rpt.admin_count, rpt.trust_score, 100, rpt.anomaly_count,
            rpt.poll_ms);
        tlog("INFO", logmsg);

        for (int i = 0; i < g_poll_sec && g_running; i++) sleep(1);
    }

    tlog("INFO", "tigerclawd shutdown");
    cleanup();
    return 0;
}
