/*
 * shredderd.c — Kernel Integrity & Root Persistence Watcher
 *
 * Every poll:
 *   - Full root-surface check: su paths, Magisk, KernelSU, LineageOS adbd
 *   - Verified boot state + verity mode delta tracking
 *   - Kernel module enumeration (loaded non-stock modules = suspicious)
 *   - Check for debugfs mount (root-level kernel access)
 *   - Scan /proc/keys for suspicious named keyrings
 *   - Count uid=0 (root) processes
 *   - Track changes between polls and produce integrity score (0-100)
 *   - Report to hub and emit APRIL events on integrity regression
 *
 * APRIL events emitted:
 *   integrity_warn   — score below SCORE_WARN
 *   root_persistence — root indicators found across consecutive polls
 *   kernel_module    — unexpected kernel module detected
 */

#include "ipc_globals.h"
#include "backend_exec.h"
#include "gaveld_emit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>

#define DAEMON_NAME      "shredderd"
#define POLL_SEC         60
#define SCORE_WARN       65
#define BUF_SIZE         2048
#define ROOT_PERSIST_MIN 2   /* consecutive polls with root → emit APRIL */

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
                         "APRIL|" DAEMON_NAME "|%s|%s\n", type, payload);
        if (n > 0) write(fd, buf, (size_t)n);
    }
    close(fd);
}

static void hub_report(const char *msg)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return;
    fcntl(fd, F_SETFL, O_NONBLOCK);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, TURTLE_SOCKET, sizeof(addr.sun_path) - 1);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0)
        write(fd, msg, strlen(msg));
    close(fd);
}

/* ── Helpers ──────────────────────────────────────────────────────────── */

static void getprop(const char *key, char *out, size_t outlen)
{
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "getprop %s 2>/dev/null", key);
    FILE *f = popen(cmd, "r");
    if (!f) { strncpy(out, "err", outlen); return; }
    out[0] = '\0';
    fgets(out, (int)outlen, f);
    pclose(f);
    out[strcspn(out, "\n\r")] = '\0';
    if (!out[0]) strncpy(out, "(unset)", outlen);
}


static int file_exists(const char *path)
{
    return access(path, F_OK) == 0;
}

static int is_debugfs_mounted(void)
{
    FILE *f = fopen("/proc/mounts", "r");
    if (!f) return 0;
    char line[256];
    int found = 0;
    while (fgets(line, sizeof(line), f))
        if (strstr(line, "debugfs")) { found = 1; break; }
    fclose(f);
    return found;
}

/* Count processes running as uid 0 */
static int count_root_procs(void)
{
    DIR *d = opendir("/proc");
    if (!d) return -1;
    struct dirent *ent;
    int cnt = 0;
    while ((ent = readdir(d)) != NULL) {
        int is_pid = 1;
        for (char *c = ent->d_name; *c; c++)
            if (*c < '0' || *c > '9') { is_pid = 0; break; }
        if (!is_pid) continue;
        char path[64];
        snprintf(path, sizeof(path), "/proc/%s/status", ent->d_name);
        FILE *f = fopen(path, "r");
        if (!f) continue;
        char line[128];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "Uid:", 4) == 0) {
                int uid = 0;
                sscanf(line + 4, " %d", &uid);
                if (uid == 0) cnt++;
                break;
            }
        }
        fclose(f);
    }
    closedir(d);
    return cnt;
}

/* Count loaded kernel modules */
static int count_modules(char *first_unfamiliar, size_t flen)
{
    if (first_unfamiliar) first_unfamiliar[0] = '\0';
    FILE *f = fopen("/proc/modules", "r");
    if (!f) return 0;
    char line[256];
    int cnt = 0;
    while (fgets(line, sizeof(line), f)) {
        cnt++;
        /* Flag modules not in a stock MIUI signature set */
        if (first_unfamiliar && !first_unfamiliar[0]) {
            /* Heuristic: magisk, ksu, frida in module name = suspicious */
            char name[64] = {0};
            sscanf(line, "%63s", name);
            if (strstr(name, "frida") ||  /* magisk/ksu are user choice, not flagged */
                strstr(name, "frida")  || strstr(name, "xposed")) {
                strncpy(first_unfamiliar, name, flen - 1);
            }
        }
    }
    fclose(f);
    return cnt;
}

/* ── State for delta tracking ─────────────────────────────────────────── */

static int g_prev_score   = -1;
static int g_root_streak  = 0;
static char g_prev_vb[32] = {0};

/* ── Poll ─────────────────────────────────────────────────────────────── */

static void poll_integrity(void)
{
    int score = 100;

    char ts[32];
    time_t t = time(NULL);
    strftime(ts, sizeof(ts), "%H:%M:%S", localtime(&t));

    printf("\n[SHREDDER] ── Kernel Integrity  %s ────────────────────────\n", ts);

    /* ── Root surface scan ────────────────────────────────────────────── */
    static const char *su_paths[] = {
        "/system/bin/su", "/system/xbin/su", "/sbin/su",
        "/su/bin/su", "/magisk/.core/bin/su", NULL
    };
    int su_found = 0;
    char su_locations[128] = {0};
    for (int i = 0; su_paths[i]; i++) {
        if (file_exists(su_paths[i])) {
            su_found++;
            if (su_locations[0])
                strncat(su_locations, " ", sizeof(su_locations)-strlen(su_locations)-1);
            strncat(su_locations, su_paths[i], sizeof(su_locations)-strlen(su_locations)-1);
        }
    }

    int magisk = file_exists("/sbin/.magisk") ||
                 file_exists("/data/adb/magisk") ||
                 file_exists("/data/adb/modules") ||
                 file_exists("/data/adb/ksu");
    int kernelsu = file_exists("/proc/kernelsu") ||
                   file_exists("/sys/kernel/ksu");

    if (su_found)  score -= 20;
    if (magisk)    score -= 15;
    if (kernelsu)  score -= 15;

    int rooted = su_found || magisk || kernelsu;
    printf("[SHREDDER]  su binaries     : %-6s  (%s)\n",
           su_found ? "FOUND" : "none", su_found ? su_locations : "clean");
    printf("[SHREDDER]  Magisk          : %-6s  KernelSU: %s\n",
           magisk ? "ACTIVE" : "absent", kernelsu ? "ACTIVE" : "absent");

    /* ── Verified boot delta ──────────────────────────────────────────── */
    char vb_state[32], vb_mode[32];
    getprop("ro.boot.verifiedbootstate", vb_state, sizeof(vb_state));
    getprop("ro.boot.veritymode",        vb_mode,  sizeof(vb_mode));
    int vb_ok = (strcmp(vb_state, "green") == 0);
    if (!vb_ok) score -= 15;

    int vb_changed = (g_prev_vb[0] && strcmp(g_prev_vb, vb_state) != 0);
    printf("[SHREDDER]  Verified Boot   : %-10s  mode=%-8s  %s%s\n",
           vb_state, vb_mode,
           vb_ok ? "ok" : "WARNING",
           vb_changed ? "  *** CHANGED" : "");
    strncpy(g_prev_vb, vb_state, sizeof(g_prev_vb) - 1);

    /* ── Kernel modules ───────────────────────────────────────────────── */
    char suspicious_mod[64] = {0};
    int  nmod = count_modules(suspicious_mod, sizeof(suspicious_mod));
    if (suspicious_mod[0]) score -= 20;
    printf("[SHREDDER]  Kernel modules  : %d loaded  %s\n",
           nmod, suspicious_mod[0] ? suspicious_mod : "(none suspicious)");
    if (suspicious_mod[0]) {
        char ev[128];
        snprintf(ev, sizeof(ev), "module=%.48s total=%d", suspicious_mod, nmod);
        gaveld_emit(DAEMON_NAME, "KERNEL_MODULE_UNKNOWN", 0.0, ev);
        splinterd_emit("kernel_module", ev);
    }

    /* ── Debugfs ──────────────────────────────────────────────────────── */
    int debugfs = is_debugfs_mounted();
    if (debugfs) score -= 5;
    printf("[SHREDDER]  debugfs mount   : %s\n", debugfs ? "MOUNTED (!)" : "absent");

    /* ── Root process count ───────────────────────────────────────────── */
    int root_procs = count_root_procs();
    if (root_procs > 10) score -= 5;
    printf("[SHREDDER]  uid=0 processes : %d\n", root_procs);

    /* ── Kernel security ──────────────────────────────────────────────── */
    char enforce[32] = "unknown";
    char *_se = bexec("getenforce 2>/dev/null");
    FILE *ef = _se ? fmemopen(_se, strlen(_se), "r") : NULL;
    if (ef) { fgets(enforce, sizeof(enforce), ef); pclose(ef); }
    enforce[strcspn(enforce, "\n")] = '\0';
    int enforcing = (strcasecmp(enforce, "Enforcing") == 0);
    if (!enforcing) score -= 15;
    printf("[SHREDDER]  SELinux         : %s\n", enforce);

    /* ── Score & grade ────────────────────────────────────────────────── */
    if (score < 0) score = 0;
    const char *grade = score >= 90 ? "INTACT"
                      : score >= 70 ? "DEGRADED"
                      : score >= 50 ? "COMPROMISED"
                      : "CRITICAL";

    printf("[SHREDDER]  ─────────────────────────────────────────────────────\n");
    printf("[SHREDDER]  Integrity Score : %d/100  [%s]\n", score, grade);
    if (g_prev_score >= 0 && score != g_prev_score)
        printf("[SHREDDER]  Score delta     : %+d  (was %d)\n",
               score - g_prev_score, g_prev_score);

    /* Root persistence streak */
    if (rooted) {
        g_root_streak++;
        if (g_root_streak >= ROOT_PERSIST_MIN) {
            char ev[256];
            snprintf(ev, sizeof(ev),
                     "streak=%d su=%d magisk=%d kernelsu=%d score=%d",
                     g_root_streak, su_found, magisk, kernelsu, score);
            gaveld_emit(DAEMON_NAME, "ROOT_PERSISTENCE", 0.0, ev);
            splinterd_emit("root_persistence", ev);
        }
    } else {
        g_root_streak = 0;
    }

    /* Hub report */
    char hub_msg[BUF_SIZE];
    snprintf(hub_msg, sizeof(hub_msg),
             "STATUS SHREDDER SELINUX=%s VB_STATE=%s VB_MODE=%s "
             "SU=%d MAGISK=%d KERNELSU=%d MODULES=%d DEBUGFS=%d "
             "ROOT_PROCS=%d SCORE=%d GRADE=%s\n",
             enforce, vb_state, vb_mode,
             su_found, magisk, kernelsu,
             nmod, debugfs, root_procs, score, grade);
    hub_report(hub_msg);

    /* APRIL: integrity warn */
    if (score < SCORE_WARN) {
        char ev[256];
        snprintf(ev, sizeof(ev),
                 "score=%d grade=%.12s rooted=%d modules=%d",
                 score, grade, rooted, nmod);
        gaveld_emit(DAEMON_NAME, "INTEGRITY_SCORE_LOW", 0.0, ev);
        splinterd_emit("integrity_warn", ev);
    }

    g_prev_score = score;
}

/* ── Main ─────────────────────────────────────────────────────────────── */

int main(void)
{
    bexec_init();
    printf("[SHREDDER] Kernel Integrity & Root Persistence Watcher: ONLINE\n");
    printf("[SHREDDER] Poll interval: %ds\n", POLL_SEC);

    for (;;) {
        poll_integrity();
        sleep(POLL_SEC);
    }

    return 0;
}
