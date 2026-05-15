/*
 * granitord.c — Security Posture & Boot Integrity Checker
 *
 * Every poll:
 *   - Read SELinux enforcement state
 *   - Verify verified boot state, verity mode, flash lock
 *   - Check ro.secure, ro.debuggable, encryption state
 *   - Detect root indicators (/system/bin/su, /sbin/su, /system/xbin/su)
 *   - Check kernel security parameters from /proc/sys/kernel/
 *   - Scan debugfs and perf_event_paranoid settings
 *   - Calculate a 0–100 security posture score
 *   - Report to turtlecomd hub and emit APRIL events on regressions
 *
 * APRIL events emitted:
 *   security_warn    — posture score below SCORE_WARN
 *   root_detected    — su binary or Magisk path found
 *   selinux_permissive — SELinux not enforcing
 */

#include "ipc_globals.h"
#include "backend_exec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>

#define DAEMON_NAME  "granitord"
#define POLL_SEC     30
#define SCORE_WARN   60
#define BUF_SIZE     1024

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
    snprintf(cmd, sizeof(cmd), "getprop %s", key);
    char *r = bexec(cmd);
    if (!r) { strncpy(out, "err", outlen); return; }
    strncpy(out, r, outlen - 1);
    out[outlen - 1] = '\0';
    out[strcspn(out, "\n\r")] = '\0';
    if (!out[0]) strncpy(out, "(unset)", outlen);
    free(r);
}

static long read_long_file(const char *path)
{
    char *s = bexec_read_file(path);
    if (!s) return -1;
    long v = atol(s);
    free(s);
    return v;
}

static int file_exists(const char *path)
{
    return access(path, F_OK) == 0;
}

/* ── Poll ─────────────────────────────────────────────────────────────── */

static void poll_security(void)
{
    int score = 100;    /* start perfect, deduct for issues */

    char ts[32];
    time_t t = time(NULL);
    strftime(ts, sizeof(ts), "%H:%M:%S", localtime(&t));

    printf("\n[GRANITOR] ── Security Posture  %s ─────────────────────────\n", ts);

    /* ── SELinux ─────────────────────────────────────────────────────── */
    char selinux[32];
    getprop("ro.boot.selinux", selinux, sizeof(selinux));
    /* Live SELinux enforcement via privileged backend */
    char enforce[32] = "unknown";
    char *ge = bexec("getenforce");
    if (ge) {
        strncpy(enforce, ge, sizeof(enforce) - 1);
        enforce[strcspn(enforce, "\n")] = '\0';
        free(ge);
    }
    int enforcing = (strcasecmp(enforce, "Enforcing") == 0);
    if (!enforcing) { score -= 25; }
    printf("[GRANITOR]  SELinux         : %-12s  (boot: %s)   %s\n",
           enforce, selinux, enforcing ? "ok" : "!!! PERMISSIVE");

    /* ── Verified Boot ───────────────────────────────────────────────── */
    char vb_state[32], vb_mode[32], flash_lock[16];
    getprop("ro.boot.verifiedbootstate", vb_state,  sizeof(vb_state));
    getprop("ro.boot.veritymode",        vb_mode,   sizeof(vb_mode));
    getprop("ro.boot.flash.locked",      flash_lock, sizeof(flash_lock));
    int vb_ok   = (strcmp(vb_state, "green") == 0);
    int fl_ok   = (strcmp(flash_lock, "1")   == 0);
    if (!vb_ok) score -= 15;
    if (!fl_ok) score -= 10;
    printf("[GRANITOR]  Verified Boot   : %-12s  mode=%-8s  flash_locked=%-4s  %s\n",
           vb_state, vb_mode, flash_lock,
           (vb_ok && fl_ok) ? "ok" : "WARNING");

    /* ── ro.secure / ro.debuggable ───────────────────────────────────── */
    char ro_secure[8], ro_debug[8], ro_encrypt[32];
    getprop("ro.secure",          ro_secure,  sizeof(ro_secure));
    getprop("ro.debuggable",      ro_debug,   sizeof(ro_debug));
    getprop("ro.crypto.state",    ro_encrypt, sizeof(ro_encrypt));
    int secure_ok = (strcmp(ro_secure, "1") == 0);
    int debug_bad = (strcmp(ro_debug, "1")  == 0);
    if (!secure_ok) score -= 10;
    if ( debug_bad) score -= 10;
    printf("[GRANITOR]  ro.secure       : %-6s  ro.debuggable: %-5s  encryption: %s  %s\n",
           ro_secure, ro_debug, ro_encrypt,
           (secure_ok && !debug_bad) ? "ok" : "WARNING");

    /* ── Root indicators ─────────────────────────────────────────────── */
    int has_su = file_exists("/system/bin/su")  ||
                 file_exists("/system/xbin/su") ||
                 file_exists("/sbin/su")         ||
                 file_exists("/su/bin/su");
    int has_magisk = file_exists("/sbin/.magisk")      ||
                     file_exists("/data/adb/magisk")   ||
                     file_exists("/data/adb/modules");
    int rooted = has_su || has_magisk;
    if (rooted) score -= 20;
    printf("[GRANITOR]  Root / su       : %-6s  Magisk: %-6s  %s\n",
           has_su    ? "FOUND" : "none",
           has_magisk ? "FOUND" : "none",
           rooted ? "!!! ROOTED" : "ok");

    /* ── Kernel security parameters ──────────────────────────────────── */
    long kptr_restrict   = read_long_file("/proc/sys/kernel/kptr_restrict");
    long dmesg_restrict  = read_long_file("/proc/sys/kernel/dmesg_restrict");
    long perf_paranoid   = read_long_file("/proc/sys/kernel/perf_event_paranoid");
    long aslr            = read_long_file("/proc/sys/kernel/randomize_va_space");
    long hardlinks       = read_long_file("/proc/sys/fs/protected_hardlinks");
    long symlinks        = read_long_file("/proc/sys/fs/protected_symlinks");

    /* Only penalise if readable AND explicitly misconfigured (value=0).
     * -1 means the file is restricted on MIUI — treat as neutral. */
    if (kptr_restrict  >= 0 && kptr_restrict  < 1) score -= 2;
    if (dmesg_restrict >= 0 && dmesg_restrict < 1) score -= 2;
    if (aslr           >= 0 && aslr           < 2) score -= 3;
    if (hardlinks      >= 0 && !hardlinks)          score -= 1;
    if (symlinks       >= 0 && !symlinks)           score -= 1;

    /* Helper: display value or "N/A (MIUI-restricted)" */
    #define PV(v) ((v) >= 0 ? (v) : -1L)
    #define PS(v) ((v) >= 0 ? "" : " (restricted)")

    printf("[GRANITOR]  Kernel params\n");
    printf("[GRANITOR]    kptr_restrict=%-2ld%s  dmesg_restrict=%-2ld%s  "
           "perf_paranoid=%-2ld%s  ASLR=%-2ld%s\n",
           PV(kptr_restrict), PS(kptr_restrict),
           PV(dmesg_restrict), PS(dmesg_restrict),
           PV(perf_paranoid),  PS(perf_paranoid),
           PV(aslr),           PS(aslr));
    printf("[GRANITOR]    hardlinks=%-2ld%s  symlinks=%-2ld%s\n",
           PV(hardlinks), PS(hardlinks),
           PV(symlinks),  PS(symlinks));

    #undef PV
    #undef PS

    /* ── Score summary ───────────────────────────────────────────────── */
    if (score < 0) score = 0;
    const char *grade = score >= 90 ? "SECURE"
                      : score >= 70 ? "CAUTION"
                      : score >= 50 ? "AT RISK"
                      : "COMPROMISED";
    printf("[GRANITOR]  ────────────────────────────────────────────────────\n");
    printf("[GRANITOR]  Security Score  : %d/100  [%s]\n", score, grade);

    /* Hub report */
    char hub_msg[BUF_SIZE];
    snprintf(hub_msg, sizeof(hub_msg),
             "STATUS GRANITOR SELINUX=%s VBOOT=%s FLASH_LOCKED=%s "
             "SECURE=%s DEBUGGABLE=%s ENCRYPT=%s ROOT=%s MAGISK=%s SCORE=%d\n",
             enforce, vb_state, flash_lock,
             ro_secure, ro_debug, ro_encrypt,
             has_su ? "yes" : "no",
             has_magisk ? "yes" : "no",
             score);
    hub_report(hub_msg);

    /* APRIL events */
    if (!enforcing) {
        splinterd_emit("selinux_permissive", "mode=permissive");
    }
    if (rooted) {
        char ev[128];
        snprintf(ev, sizeof(ev),
                 "su=%d magisk=%d score=%d", has_su, has_magisk, score);
        splinterd_emit("root_detected", ev);
    }
    if (score < SCORE_WARN) {
        char ev[128];
        snprintf(ev, sizeof(ev),
                 "score=%d grade=%.12s vboot=%.16s", score, grade, vb_state);
        splinterd_emit("security_warn", ev);
    }
}

/* ── Main ─────────────────────────────────────────────────────────────── */

int main(void)
{
    bexec_init();
    printf("[GRANITOR] Security & Boot Integrity Checker: ONLINE\n");
    printf("[GRANITOR] Poll interval: %ds\n", POLL_SEC);

    for (;;) {
        poll_security();
        sleep(POLL_SEC);
    }

    return 0;
}
