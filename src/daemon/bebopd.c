/*
 * bebopd.c -- Wakelock & Power Drain Audit Daemon
 *
 * Redmi 15C ? HyperOS OS2.0 ? Android 15
 *
 * Every poll:
 *   - Parse dumpsys power for active wakelocks and suspend blockers
 *   - Detect full wakelocks held by non-system processes
 *   - Parse battery drain stats (interactive mAh/h)
 *   - Detect orphaned InCall wakelocks (no active call)
 *   - Count doze interruptions from wakelock history
 *   - Check battery level and saver state
 *   - Score power posture 0-100
 *   - Emit gaveld signals and write results
 *
 * Adaptive:
 *   - Parses whatever wakelocks dumpsys power reports
 *   - Works on any Android device with dumpsys power access
 *   - Degrades gracefully if dumpsys unavailable
 *
 * Gaveld signals:
 *   WAKELOCK_ANOMALY, WAKELOCK_FULL_HELD, WAKELOCK_DRAIN_HIGH,
 *   DOZE_INTERRUPTED, INCALL_WAKELOCK_ORPHAN,
 *   BATTERY_LEVEL_CRITICAL, BATTERY_SAVER_OFF_LOW
 *
 * Runtime config: enabled, interval (default 30), scan_count
 */

#include "ipc_globals.h"
#include "gaveld_emit.h"
#include "backend_exec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdbool.h>
#include <stdarg.h>

#define DAEMON_NAME         "bebopd"
#define DEFAULT_INTERVAL    30
#define DRAIN_WARN_MAH_H    400.0f   /* interactive mAh/h = anomaly */
#define FULL_WAKELOCK_WARN  2        /* non-system full wakelocks = warn */
#define BATTERY_CRITICAL    10       /* % */
#define SYSTEM_UID_MAX      9999     /* UIDs <= this are system */

#ifndef MP_BASE_DIR
#define MP_BASE_DIR "/data/data/com.termux/files/home/MiuiserPeruser"
#endif

#define STATE_FILE   MP_BASE_DIR "/Registry/daemon_state.json"
#define RESULTS_DIR  MP_BASE_DIR "/Registry/daemon_results"
#define RESULTS_FILE RESULTS_DIR "/" DAEMON_NAME ".json"

/* ?? Wakelock record ???????????????????????????????????????????????????? */

typedef struct {
    char  tag[80];
    int   uid;
    char  type[16];     /* FULL, PARTIAL, SCREEN_BRIGHT, etc. */
    bool  is_system;
    bool  is_full;
} wakelock_t;

/* ?? Config ????????????????????????????????????????????????????????????? */

static int config_get_int(const char *key, int def)
{
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "jq -r '.%s.%s // %d' %s 2>/dev/null",
             DAEMON_NAME, key, def, STATE_FILE);
    FILE *f = popen(cmd, "r");
    if (!f) return def;
    char buf[32] = {0};
    int val = def;
    if (fgets(buf, sizeof(buf), f) && buf[0] != 'n')
        val = atoi(buf);
    pclose(f);
    return val;
}

static int is_enabled(void)    { return config_get_int("enabled",    1); }
static int get_interval(void)  { return config_get_int("interval",   DEFAULT_INTERVAL); }
static int get_max_scans(void) { return config_get_int("scan_count", 0); }

/* ?? Splinterd emit ????????????????????????????????????????????????????? */

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

/* -- Logging ---------------------------------------------------------- */

static void beboplog(const char *level, const char *fmt, ...);

static void beboplog(const char *level, const char *fmt, ...)
{
    time_t t = time(NULL);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&t));

    va_list args;
    va_start(args, fmt);
    printf("[%s] [BEBOP/%s] ", ts, level);
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

/* -- Field sanitization ------------------------------------------------ */
/* Verbatim copy from fugitoidd.c/shredderd.c/granitord.c. Neutralizes
 * control chars, '"', '|', and '\' so device-sourced strings can't
 * break JSON fields or the '|'-delimited APRIL/gaveld wire protocol. */

static void sanitize_field(char *s)
{
    if (!s) return;
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c < 0x20 || c == '"' || c == '|' || c == '\\')
            *s = '_';
    }
}

/* ?? Parse wakelocks from dumpsys power ?????????????????????????????????? */

/*
 * Parses "Wake Locks: size=N" section.
 * Each entry looks like:
 *   SCREEN_BRIGHT_WAKE_LOCK  'WindowManager/displayId:0' ON_AFTER_RELEASE
 *   ACQ=-1s508ms (uid=1000 pid=1650 pkg=android ws=WorkSource{...})
 */
static int parse_wakelocks(const char *dump,
                             wakelock_t *wls, int max_wls)
{
    if (!dump) return 0;

    const char *section = strstr(dump, "Wake Locks:");
    if (!section) return 0;

    int count = 0;
    const char *p = section;

    while (*p && count < max_wls) {
        /* Find a line with wakelock type keywords */
        const char *line = p;
        const char *nl   = strchr(p, '\n');
        if (!nl) break;

        char lbuf[512];
        size_t llen = (size_t)(nl - line);
        if (llen >= sizeof(lbuf)) llen = sizeof(lbuf) - 1;
        strncpy(lbuf, line, llen);
        lbuf[llen] = '\0';

        p = nl + 1;

        /* Skip section headers and empty lines */
        if (!strstr(lbuf, "WAKE_LOCK") && !strstr(lbuf, "ACQ="))
            continue;
        /* Stop at next major section */
        if (strstr(lbuf, "Suspend Blockers:") ||
            strstr(lbuf, "Battery saving"))
            break;

        if (!strstr(lbuf, "WAKE_LOCK")) continue;

        wakelock_t *wl = &wls[count];
        memset(wl, 0, sizeof(*wl));

        /* UID: check current line first -- live dumpsys power on this
         * device puts "uid=" inline on the wakelock entry line itself.
         * Must happen before strtok() mutates lbuf below. */
        int uid_found = 0;
        const char *uid_p = strstr(lbuf, "uid=");
        if (uid_p) {
            wl->uid = atoi(uid_p + 4);
            uid_found = 1;
        }

        /* Type: first token on line */
        char *tok = strtok(lbuf, " \t");
        if (!tok) continue;
        strncpy(wl->type, tok, sizeof(wl->type) - 1);

        wl->is_full = (strstr(wl->type, "FULL") != NULL ||
                       strstr(wl->type, "PARTIAL") != NULL);

        /* Tag: in single quotes. Search from the end of the actual
         * token (tok), not lbuf -- lbuf may have leading whitespace
         * strtok() skipped past, and lbuf + strlen(tok) can land at or
         * before the NUL strtok() just inserted after the token, which
         * makes strchr() hit the terminator before reaching the quote. */
        char *tq = strchr(tok + strlen(tok) + 1, '\'');
        if (tq) {
            tq++;
            size_t i = 0;
            while (*tq && *tq != '\'' && i < sizeof(wl->tag) - 1)
                wl->tag[i++] = *tq++;
            wl->tag[i] = '\0';
        }

        /* UID fallback: older/other dumpsys formats may still put
         * ACQ=/uid= on the line after the wakelock entry. Only used if
         * the current-line check above found nothing. */
        if (!uid_found) {
            const char *acq_line = p;
            const char *acq_nl   = strchr(p, '\n');
            if (acq_nl) {
                char acq[256];
                size_t alen = (size_t)(acq_nl - acq_line);
                if (alen >= sizeof(acq)) alen = sizeof(acq) - 1;
                strncpy(acq, acq_line, alen);
                acq[alen] = '\0';

                const char *uid_p2 = strstr(acq, "uid=");
                if (uid_p2) wl->uid = atoi(uid_p2 + 4);
            }
        }

        wl->is_system = (wl->uid <= SYSTEM_UID_MAX);
        count++;
    }

    return count;
}

/* ?? Parse battery level from dumpsys power ???????????????????????????? */

static int parse_battery_level(const char *dump)
{
    if (!dump) return -1;
    const char *p = strstr(dump, "level:");
    if (!p) return -1;
    p += 6; while (*p == ' ') p++;
    return atoi(p);
}

static bool parse_battery_low(const char *dump)
{
    if (!dump) return false;
    const char *p = strstr(dump, "mBatteryLevelLow=");
    if (!p) return false;
    return strncmp(p + 17, "true", 4) == 0;
}

static bool parse_battery_saver(const char *dump)
{
    if (!dump) return false;
    /* "Battery Saver is currently: OFF" or "ON" */
    const char *p = strstr(dump, "Battery Saver is currently:");
    if (!p) return false;
    p += 27;
    while (*p == ' ') p++;
    return strncmp(p, "ON", 2) == 0;
}

/* ?? Parse interactive drain mAh/h ????????????????????????????????????? */

/*
 * Looks for lines like:
 *   NonDoze NonIntr:    275m    274mAh(  5%)     59.8mAh/h
 *             Intr:   1306m  10999mAh(186%)    505.0mAh/h
 * Returns the interactive (Intr) NonDoze mAh/h figure.
 */
static float parse_drain_mah_h(const char *dump)
{
    if (!dump) return 0.0f;

    /* HyperOS format: "  Computed drain: 2320, actual drain: 2058-2176" */
    const char *cd = strstr(dump, "Computed drain:");
    if (cd) {
        cd += strlen("Computed drain:");
        while (*cd == ' ') cd++;
        return strtof(cd, NULL);
    }

    /* Fallback: "Screen off discharge: 680 mAh" */
    const char *sd = strstr(dump, "Screen off discharge:");
    if (sd) {
        sd += strlen("Screen off discharge:");
        while (*sd == ' ') sd++;
        return strtof(sd, NULL);
    }

    return 0.0f;
}

/* ?? Parse doze interruptions ??????????????????????????????????????????? */

static int count_doze_interrupted(const char *dump)
{
    if (!dump) return 0;
    int count = 0;
    const char *p = dump;
    while ((p = strstr(p, "Intr:")) != NULL) {
        /* Check it's in a doze context (Light or Deep section) */
        count++;
        p++;
    }
    /* Rough heuristic -- subtract the one in NonDoze section */
    return count > 1 ? count - 1 : 0;
}

/* ?? Results writer ????????????????????????????????????????????????????? */

static void write_results(int score, int scan_num, int sigs,
                           int battery, float drain, int wl_count,
                           int full_nonsys)
{
    FILE *f = fopen(RESULTS_FILE, "w");
    if (!f) return;

    time_t t = time(NULL);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", localtime(&t));

    const char *grade = score >= 85 ? "CLEAN"
                      : score >= 65 ? "ACTIVE"
                      : score >= 45 ? "DRAINING"
                      : "CRITICAL";

    fprintf(f,
        "{\n"
        "  \"daemon\": \"" DAEMON_NAME "\",\n"
        "  \"timestamp\": \"%s\",\n"
        "  \"scan_number\": %d,\n"
        "  \"signals_fired\": %d,\n"
        "  \"power_score\": %d,\n"
        "  \"grade\": \"%s\",\n"
        "  \"battery_pct\": %d,\n"
        "  \"drain_mah_h\": %.1f,\n"
        "  \"active_wakelocks\": %d,\n"
        "  \"full_nonsystem_wakelocks\": %d\n"
        "}\n",
        ts, scan_num, sigs, score, grade,
        battery, drain, wl_count, full_nonsys);

    fclose(f);
}

/* ?? Main poll ?????????????????????????????????????????????????????????? */

static void poll(int scan_num)
{
    char ts[32];
    time_t t = time(NULL);
    strftime(ts, sizeof(ts), "%H:%M:%S", localtime(&t));

    beboplog("INFO", "\n?? Wakelock & Power Scan #%d  %s ?????????????????",
           scan_num, ts);

    int score = 100;
    int sigs  = 0;

    /* ?? Dump power ????????????????????????????????????????????????????? */
    char *dump = bexec("dumpsys power 2>/dev/null");
    char *bstats = bexec("dumpsys batterystats | grep -E Computed.drain | head -5");
    if (!dump) {
        beboplog("WARN", "dumpsys power unavailable");
        return;
    }

    /* ?? Battery state ?????????????????????????????????????????????????? */
    char *batdump = bexec("dumpsys battery");
    int  battery     = parse_battery_level(batdump ? batdump : dump);
    if (batdump) free(batdump);
    bool battery_low = parse_battery_low(dump);
    bool saver_on    = parse_battery_saver(dump);
    float drain      = parse_drain_mah_h(bstats ? bstats : dump);

    beboplog("INFO", "Battery    : %d%%  low=%s  saver=%s",
           battery, battery_low ? "yes" : "no", saver_on ? "ON" : "off");
    beboplog("INFO", "Drain      : %.1f mAh/h (interactive)", drain);

    /* Battery critical */
    if (battery >= 0 && battery <= BATTERY_CRITICAL) {
        char ctx[32];
        snprintf(ctx, sizeof(ctx), "level=%d%%", battery);
        gaveld_emit(DAEMON_NAME, "BATTERY_LEVEL_CRITICAL", 0.0, ctx);
        splinterd_emit("BATTERY_LEVEL_CRITICAL", ctx);
        score -= 15;
        sigs++;
        beboplog("WARN", "Battery critical: %d%%", battery);
    }

    /* Battery low + saver off */
    if (battery_low && !saver_on) {
        gaveld_emit(DAEMON_NAME, "BATTERY_SAVER_OFF_LOW", 0.0, "saver=off");
        splinterd_emit("BATTERY_SAVER_OFF_LOW", "saver=off");
        score -= 8;
        sigs++;
        beboplog("WARN", "Battery low but Battery Saver is OFF");
    }

    /* High drain */
    if (drain >= DRAIN_WARN_MAH_H) {
        char ctx[48];
        snprintf(ctx, sizeof(ctx), "drain=%.1f_mAh_h threshold=%.0f",
                 drain, DRAIN_WARN_MAH_H);
        gaveld_emit(DAEMON_NAME, "WAKELOCK_DRAIN_HIGH", drain, ctx);
        splinterd_emit("WAKELOCK_DRAIN_HIGH", ctx);
        score -= 18;
        sigs++;
        beboplog("WARN", "High drain: %.1f mAh/h", drain);
    }

    /* ?? Wakelocks ?????????????????????????????????????????????????????? */
    wakelock_t wls[32];
    int nwls = parse_wakelocks(dump, wls, 32);

    beboplog("INFO", "Wakelocks  : %d active", nwls);
    if (nwls > 0) {
        beboplog("INFO", "%-30s  %-8s  %-10s  %s",
               "Tag", "Type", "UID", "System");
        beboplog("INFO", "?????????????????????????????????????????????????");
    }

    int full_nonsys = 0;
    bool incall_orphan = false;

    for (int i = 0; i < nwls; i++) {
        wakelock_t *wl = &wls[i];
        sanitize_field(wl->tag);
        sanitize_field(wl->type);
        beboplog("INFO", "%-30.30s  %-8.8s  uid=%-6d  %s",
               wl->tag, wl->type, wl->uid,
               wl->is_system ? "system" : "APP");

        if (!wl->is_system && wl->is_full) {
            full_nonsys++;
            char ctx[128];
            snprintf(ctx, sizeof(ctx), "tag=%.48s uid=%d type=%.12s",
                     wl->tag, wl->uid, wl->type);
            gaveld_emit(DAEMON_NAME, "WAKELOCK_FULL_HELD", 1.0, ctx);
            splinterd_emit("WAKELOCK_FULL_HELD", ctx);
            score -= 20;
            sigs++;
            beboplog("WARN", "Full wakelock by non-system: %s", wl->tag);
        }

        /* Orphaned InCall wakelock */
        if (strstr(wl->tag, "InCall") || strstr(wl->tag, "incall")) {
            incall_orphan = true;
        }
    }

    if (incall_orphan) {
        gaveld_emit(DAEMON_NAME, "INCALL_WAKELOCK_ORPHAN", 1.0,
                    "tag=InCallWakeLockController");
        splinterd_emit("INCALL_WAKELOCK_ORPHAN", "tag=InCallWakeLockController");
        score -= 20;
        sigs++;
        beboplog("WARN", "InCall wakelock active -- call in progress or orphaned");
    }

    /* General wakelock anomaly */
    if (full_nonsys >= FULL_WAKELOCK_WARN) {
        char ctx[32];
        snprintf(ctx, sizeof(ctx), "count=%d", full_nonsys);
        gaveld_emit(DAEMON_NAME, "WAKELOCK_ANOMALY", 0.0, ctx);
        splinterd_emit("WAKELOCK_ANOMALY", ctx);
        score -= 10;
        sigs++;
    }

    /* ?? Doze ??????????????????????????????????????????????????????????? */
    int doze_intr = count_doze_interrupted(dump);
    beboplog("INFO", "Doze intr  : ~%d session(s)", doze_intr);
    if (doze_intr > 2) {
        char ctx[32];
        snprintf(ctx, sizeof(ctx), "interruptions=%d", doze_intr);
        gaveld_emit(DAEMON_NAME, "DOZE_INTERRUPTED", 0.0, ctx);
        splinterd_emit("DOZE_INTERRUPTED", ctx);
        score -= 10;
        sigs++;
        beboplog("WARN", "Doze repeatedly interrupted");
    }

    free(dump);
    if (bstats) free(bstats);

    if (score < 0) score = 0;
    const char *grade = score >= 85 ? "CLEAN"
                      : score >= 65 ? "ACTIVE"
                      : score >= 45 ? "DRAINING"
                      : "CRITICAL";

    beboplog("INFO", "Power score : %d/100  [%s]  signals=%d",
           score, grade, sigs);

    write_results(score, scan_num, sigs, battery, drain, nwls, full_nonsys);
}

/* ?? Main ??????????????????????????????????????????????????????????????? */

int main(void)
{
    bexec_init();

    if (!is_enabled()) {
        beboplog("INFO", "disabled via syndicatectl -- exiting");
        return 0;
    }

    beboplog("INFO", "Wakelock & Power Drain Audit Daemon: ONLINE");
    beboplog("INFO", "Drain threshold: %.0f mAh/h  Battery critical: %d%%",
           DRAIN_WARN_MAH_H, BATTERY_CRITICAL);

    int interval  = get_interval();
    int max_scans = get_max_scans();
    int scan_num  = 0;

    for (;;) {
        if (!is_enabled()) {
            beboplog("INFO", "disabled -- stopping");
            break;
        }

        interval  = get_interval();
        max_scans = get_max_scans();
        scan_num++;

        poll(scan_num);

        if (max_scans > 0 && scan_num >= max_scans) {
            beboplog("INFO", "reached scan_count=%d -- exiting", max_scans);
            break;
        }

        beboplog("INFO", "Next scan in %ds", interval);
        sleep(interval);
    }

    return 0;
}
