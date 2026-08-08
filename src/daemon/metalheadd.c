/*
 * metalheadd.c — Sensor Service Registry & Client Auditor
 *
 * Parses dumpsys sensorservice pipe-delimited format:
 * 0xHEX) NAME | vendor | ver: N | type: desc(NUM) | perm | flags
 *
 * Routed through bexec_n() with an explicit 128KB buffer (dump is 150KB+
 * raw before grep filtering — bexec()'s default 64KB cap would truncate
 * it, so bexec_n() is called directly rather than bexec()).
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

#define DAEMON_NAME      "metalheadd"
#define DEFAULT_INTERVAL 30
#define MAX_SENSORS      64
#define CLIENT_SPIKE     4
#define CLIENT_SENSITIVE 2

#ifndef MP_BASE_DIR
#define MP_BASE_DIR "/data/data/com.termux/files/home/MiuiserPeruser"
#endif

#define STATE_FILE   MP_BASE_DIR "/Registry/daemon_state.json"
#define RESULTS_DIR  MP_BASE_DIR "/Registry/daemon_results"
#define RESULTS_FILE RESULTS_DIR "/" DAEMON_NAME ".json"

typedef struct {
    char  name[80];
    char  vendor[48];
    char  type_str[48];
    int   type_id;
    int   clients;
    bool  is_sensitive;
} sensor_t;

static sensor_t g_sensors[MAX_SENSORS];
static int      g_nsensors = 0;

/* ── Config ───────────────────────────────────────────────────────────── */

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

/* In-place mutator: strips characters that would break either the
 * APRIL|daemon|type|payload pipe-framing or a downstream JSON re-wrap.
 * Per-file local static -- copied verbatim from fugitoidd.c/shredderd.c/
 * granitord.c, not shared via header (established fleet precedent). */
static void sanitize_field(char *s) {
    if (!s) return;
    for (char *p = s; *p; p++) {
        if (*p == '"' || *p == '\\' || *p == '|' || (unsigned char)*p < 0x20) {
            *p = '_';
        }
    }
}

/* Forward declaration -- call site (line ~139) appears before the
 * definition below in file order (same issue hit in shredderd.c /
 * granitord.c's logging conversions). */
static void slog(const char *level, const char *fmt, ...);

/* Timestamped, tagged logger. Copied verbatim from shredderd.c/granitord.c's
 * slog() -- per-file local static, not shared via header. Appends its own
 * trailing newline; callers should not include one in fmt. */
static void slog(const char *level, const char *fmt, ...)
{
    time_t t = time(NULL);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", localtime(&t));

    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[%s][METAL/%s] ", ts, level);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

/* ── Type helpers ─────────────────────────────────────────────────────── */

static const char *type_name(int id)
{
    switch (id) {
        case 1:  return "Accelerometer";
        case 2:  return "Magnetic Field";
        case 3:  return "Orientation";
        case 4:  return "Gyroscope";
        case 5:  return "Light";
        case 6:  return "Pressure";
        case 8:  return "Proximity";
        case 9:  return "Gravity";
        case 10: return "Linear Accel";
        case 11: return "Rotation Vector";
        case 14: return "Mag Uncal";
        case 15: return "Game Rotation";
        case 16: return "Gyro Uncal";
        case 17: return "Sig Motion";
        case 18: return "Step Detector";
        case 19: return "Step Counter";
        case 28: return "Heart Rate";
        case 65536: return "Fingerprint";
        default: return "Other";
    }
}

static bool is_sensitive_type(int id)
{
    return (id == 1 || id == 2 || id == 3 || id == 4 ||
            id == 10 || id == 16 || id == 28 || id == 65536);
}

/* ── Fetch via bexec_n() ──────────────────────────────────────────────── *
 * 128KB explicit buffer — the raw dump runs 150KB+ before grep filtering
 * (see file header). bexec() defaults to 64KB and would silently truncate;
 * bexec_n() is called directly with an explicit cap instead.
 */

#define SENSOR_DUMP_BUFSZ (128 * 1024)

static char *fetch_sensorservice(void)
{
    char *buf = bexec_n(
        "dumpsys sensorservice 2>/dev/null | grep -E '0x[0-9a-fA-F]+\\)'",
        SENSOR_DUMP_BUFSZ);

    if (!buf || buf[0] == '\0') {
        slog("WARN", "bexec_n() returned empty dump");
    }
    return buf;
}

/* ── Parse sensor list ────────────────────────────────────────────────── */

static int parse_sensors(const char *dump)
{
    if (!dump) return 0;
    int count = 0;
    const char *p = dump;

    while (*p && count < MAX_SENSORS) {
        const char *nl = strchr(p, '\n');
        char line[512] = {0};
        size_t llen = nl ? (size_t)(nl - p) : strlen(p);
        if (llen >= sizeof(line)) llen = sizeof(line) - 1;
        strncpy(line, p, llen);
        p = nl ? nl + 1 : p + llen;

        /* Need pipe delimiter for Sensor List entries */
        if (!strstr(line, "|")) continue;
        /* Skip active-count lines */
        if (strstr(line, "active-count")) continue;
        /* Need ") " to start */
        const char *name_start = strstr(line, ") ");
        if (!name_start) continue;
        name_start += 2;

        sensor_t *s = &g_sensors[count];
        memset(s, 0, sizeof(*s));

        /* Name: up to first | */
        const char *pipe1 = strchr(name_start, '|');
        if (!pipe1) continue;
        size_t nlen = (size_t)(pipe1 - name_start);
        while (nlen > 0 && name_start[nlen-1] == ' ') nlen--;
        if (nlen == 0 || nlen >= sizeof(s->name)) continue;
        strncpy(s->name, name_start, nlen);

        /* Vendor: between first and second | */
        const char *vstart = pipe1 + 1;
        while (*vstart == ' ') vstart++;
        const char *pipe2 = strchr(vstart, '|');
        if (pipe2) {
            size_t vlen = (size_t)(pipe2 - vstart);
            while (vlen > 0 && vstart[vlen-1] == ' ') vlen--;
            if (vlen < sizeof(s->vendor))
                strncpy(s->vendor, vstart, vlen);
        }

        /* Type number: from "type: ...(NUM)" */
        const char *tp = strstr(line, "type:");
        if (tp) {
            const char *paren = strchr(tp, '(');
            if (paren) {
                s->type_id = atoi(paren + 1);
            }
        }

        strncpy(s->type_str, type_name(s->type_id), sizeof(s->type_str) - 1);
        s->is_sensitive = is_sensitive_type(s->type_id);
        count++;
    }
    return count;
}

/* ── Results writer ───────────────────────────────────────────────────── */

static void write_results(int score, int scan_num, int sigs,
                           int nsensors, int sensitive_active, int flood)
{
    FILE *f = fopen(RESULTS_FILE, "w");
    if (!f) return;
    time_t t = time(NULL);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", localtime(&t));
    const char *grade = score >= 85 ? "CLEAN"
                      : score >= 65 ? "ACTIVE"
                      : score >= 45 ? "SUSPICIOUS"
                      : "COMPROMISED";
    fprintf(f,
        "{\n"
        "  \"daemon\": \"" DAEMON_NAME "\",\n"
        "  \"timestamp\": \"%s\",\n"
        "  \"scan_number\": %d,\n"
        "  \"signals_fired\": %d,\n"
        "  \"sensor_score\": %d,\n"
        "  \"grade\": \"%s\",\n"
        "  \"total_sensors\": %d,\n"
        "  \"sensitive_active\": %d,\n"
        "  \"flood_sensors\": %d\n"
        "}\n",
        ts, scan_num, sigs, score, grade,
        nsensors, sensitive_active, flood);
    fclose(f);
}

/* ── Main poll ────────────────────────────────────────────────────────── */

static void poll(int scan_num)
{
    char ts[32];
    time_t t = time(NULL);
    strftime(ts, sizeof(ts), "%H:%M:%S", localtime(&t));

    printf("\n[METAL] -- Sensor Registry Scan #%d  %s ----------------\n",
           scan_num, ts);

    int score = 100;
    int sigs  = 0;

    char *dump = fetch_sensorservice();
    g_nsensors = parse_sensors(dump);

    if (g_nsensors == 0) {
        printf("[METAL]  No sensors parsed — dump_len=%zu\n", dump ? strlen(dump) : 0);
        if (dump) free(dump);
        FILE *f = fopen(RESULTS_FILE, "w");
        if (f) {
            time_t t = time(NULL);
            char ts[32];
            strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", localtime(&t));
            fprintf(f,
                "{\n"
                "  \"daemon\": \"" DAEMON_NAME "\",\n"
                "  \"timestamp\": \"%s\",\n"
                "  \"scan_number\": %d,\n"
                "  \"signals_fired\": 0,\n"
                "  \"sensor_score\": null,\n"
                "  \"grade\": \"NO_DATA\",\n"
                "  \"total_sensors\": 0,\n"
                "  \"sensitive_active\": 0,\n"
                "  \"flood_sensors\": 0\n"
                "}\n",
                ts, scan_num);
            fclose(f);
        }
        return;
    }

    printf("[METAL]  %-4s  %-22s  %-14s  %s\n",
           "Type", "Name", "Vendor", "Sensitive");
    printf("[METAL]  --------------------------------------------------------\n");

    int sensitive_active = 0;

    for (int i = 0; i < g_nsensors; i++) {
        sensor_t *s = &g_sensors[i];
        const char *sen = s->is_sensitive ? "*" : " ";

        printf("[METAL]  %4d  %-22.22s  %-14.14s  %s\n",
               s->type_id, s->name, s->vendor, sen);

        if (s->is_sensitive) {
            sensitive_active++;
            if (sensitive_active >= CLIENT_SENSITIVE) {
                char ctx[128];
                snprintf(ctx, sizeof(ctx),
                         "sensor=%.32s type=%.16s",
                         s->name, s->type_str);
                sanitize_field(ctx);
                gaveld_emit(DAEMON_NAME, "SENSITIVE_SENSOR_ACTIVE", 0.0, ctx);
                splinterd_emit("SENSITIVE_SENSOR_ACTIVE", ctx);
                score -= 10;
                sigs++;
            }
        }
    }

    printf("[METAL]  --------------------------------------------------------\n");
    printf("[METAL]  Total: %d  Sensitive: %d\n", g_nsensors, sensitive_active);

    if (dump) free(dump);
    if (score < 0) score = 0;

    const char *grade = score >= 85 ? "CLEAN"
                      : score >= 65 ? "ACTIVE"
                      : score >= 45 ? "SUSPICIOUS"
                      : "COMPROMISED";

    printf("[METAL]  Score: %d/100  [%s]  signals=%d\n", score, grade, sigs);
    write_results(score, scan_num, sigs, g_nsensors, sensitive_active, 0);
}

/* ── Main ─────────────────────────────────────────────────────────────── */

int main(void)
{
    bexec_init();

    if (!is_enabled()) {
        printf("[METAL] disabled — exiting\n");
        return 0;
    }

    printf("[METAL] Sensor Service Registry & Client Auditor: ONLINE\n");

    int interval  = get_interval();
    int max_scans = get_max_scans();
    int scan_num  = 0;

    for (;;) {
        if (!is_enabled()) break;
        interval  = get_interval();
        max_scans = get_max_scans();
        scan_num++;
        poll(scan_num);
        if (max_scans > 0 && scan_num >= max_scans) break;
        printf("[METAL] Next scan in %ds\n", interval);
        sleep(interval);
    }

    return 0;
}
