/*
 * leatherheadd.c -- Thermal Truth & HAL Integrity Daemon
 *
 * Redmi 15C ? HyperOS OS2.0 ? MediaTek ? Android 15
 *
 * Every poll:
 *   - Parse dumpsys thermalservice for HAL temps vs cached temps
 *   - Compute delta between HAL truth and Android framework cache
 *   - Flag HAL_CACHED mismatch when delta > MISMATCH_THRESHOLD
 *   - Read CPU freq across all 8 cores (cpu0-7 via cpufreq sysfs)
 *   - Detect throttling per cluster (efficiency cpu0-3, performance cpu4-7)
 *   - Score thermal posture 0-100
 *   - Emit gaveld signals and write results
 *
 * Adaptive -- works on any device:
 *   - Enumerates thermal types dynamically from dumpsys output
 *   - Enumerates CPU cores dynamically from /sys/devices/system/cpu/
 *   - Never hardcodes zone numbers or sensor names
 *
 * Gaveld signals:
 *   THERMAL_HAL_MISMATCH, THERMAL_WARN, THERMAL_CRITICAL,
 *   CPU_THROTTLING, SKIN_TEMP_HIGH, BATTERY_TEMP_HIGH,
 *   THERMAL_STATUS_NONZERO, ALL_CORES_MAXED
 *
 * Runtime config (Registry/daemon_state.json):
 *   enabled, interval (default 30), scan_count
 */

#include "ipc_globals.h"
#include "gaveld_emit.h"
#include "backend_exec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdbool.h>

#define DAEMON_NAME          "leatherheadd"
#define DEFAULT_INTERVAL     30
#define MISMATCH_THRESHOLD   5.0f   /* degC delta HAL vs cached = suspicious */
#define SKIN_WARN_C          42.0f
#define SKIN_CRITICAL_C      48.0f
#define BATTERY_WARN_C       40.0f
#define MAX_TEMP_TYPES       16
#define MAX_CPU_CORES        16

#ifndef MP_BASE_DIR
#define MP_BASE_DIR "/data/data/com.termux/files/home/MiuiserPeruser"
#endif

#define STATE_FILE   MP_BASE_DIR "/Registry/daemon_state.json"
#define RESULTS_DIR  MP_BASE_DIR "/Registry/daemon_results"
#define RESULTS_FILE RESULTS_DIR "/" DAEMON_NAME ".json"

/* ?? Temp record ???????????????????????????????????????????????????????? */

typedef struct {
    char  name[32];
    int   type;
    float cached;   /* Android framework cached value */
    float hal;      /* HAL reported value             */
    float delta;    /* hal - cached                   */
    int   status;   /* thermal status 0=ok            */
} temp_t;

/* ?? CPU core record ???????????????????????????????????????????????????? */

typedef struct {
    int  core;
    long cur_khz;
    long max_khz;
    long min_khz;
    char governor[32];
    bool throttled;   /* cur < max * THROTTLE_RATIO */
} cpu_core_t;

#define THROTTLE_RATIO 0.75f

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

/* ?? Parse dumpsys thermalservice ??????????????????????????????????????? */

/*
 * Parses two sections:
 *   "Cached temperatures:"
 *     Temperature{mValue=53.4, mType=0, mName=CPU, mStatus=0}
 *   "Current temperatures from HAL:"
 *     Temperature{mValue=40.6, mType=0, mName=CPU, mStatus=0}
 *
 * Device-adaptive: we don't assume which types exist.
 * Returns number of temp types found.
 */
static int parse_thermalservice(const char *dump,
                                 temp_t *temps, int max_temps)
{
    if (!dump) return 0;
    int count = 0;

    /* ?? Pass 1: cached temps ??????????????????????????????????????????? */
    const char *cached_start = strstr(dump, "Cached temperatures:");
    if (!cached_start) cached_start = dump;

    const char *hal_start = strstr(dump, "Current temperatures from HAL:");

    const char *p = cached_start;
    while ((hal_start == NULL || p < hal_start) && count < max_temps) {
        const char *rec = strstr(p, "Temperature{");
        if (!rec) break;
        if (hal_start && rec >= hal_start) break;

        temp_t *t = &temps[count];
        memset(t, 0, sizeof(*t));

        /* mValue */
        const char *mv = strstr(rec, "mValue=");
        if (mv) t->cached = strtof(mv + 7, NULL);

        /* mType */
        const char *mt = strstr(rec, "mType=");
        if (mt) t->type = atoi(mt + 6);

        /* mName */
        const char *mn = strstr(rec, "mName=");
        if (mn) {
            mn += 6;
            size_t i = 0;
            while (*mn && *mn != ',' && *mn != '}' && i < sizeof(t->name) - 1)
                t->name[i++] = *mn++;
            t->name[i] = '\0';
        }

        /* mStatus */
        const char *ms = strstr(rec, "mStatus=");
        if (ms) t->status = atoi(ms + 8);

        count++;
        p = rec + 1;
    }

    /* ?? Pass 2: HAL temps -- match by name ????????????????????????????? */
    if (hal_start) {
        p = hal_start;
        while (*p) {
            const char *rec = strstr(p, "Temperature{");
            if (!rec) break;

            char hal_name[32] = {0};
            float hal_val = 0.0f;

            const char *mv = strstr(rec, "mValue=");
            if (mv) hal_val = strtof(mv + 7, NULL);

            const char *mn = strstr(rec, "mName=");
            if (mn) {
                mn += 6;
                size_t i = 0;
                while (*mn && *mn != ',' && *mn != '}' && i < sizeof(hal_name) - 1)
                    hal_name[i++] = *mn++;
                hal_name[i] = '\0';
            }

            /* Match against cached entries */
            for (int i = 0; i < count; i++) {
                if (strcmp(temps[i].name, hal_name) == 0) {
                    temps[i].hal   = hal_val;
                    temps[i].delta = hal_val - temps[i].cached;
                    break;
                }
            }

            p = rec + 1;
        }
    }

    return count;
}

/* ?? Parse thermal status ??????????????????????????????????????????????? */

static int parse_thermal_status(const char *dump)
{
    if (!dump) return 0;
    const char *p = strstr(dump, "Thermal Status:");
    if (!p) return 0;
    p += 15;
    while (*p == ' ') p++;
    return atoi(p);
}

/* ?? Read CPU cores from sysfs ????????????????????????????????????????? */

static long read_long_sysfs(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    long v = -1;
    fscanf(f, "%ld", &v);
    fclose(f);
    return v;
}

static int enumerate_cpus(cpu_core_t *cores, int max_cores)
{
    int count = 0;
    char path[256];

    for (int i = 0; i < max_cores; i++) {
        snprintf(path, sizeof(path),
                 "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", i);
        long cur = read_long_sysfs(path);
        if (cur < 0) break; /* no more cores */

        cpu_core_t *c = &cores[count];
        c->core = i;
        c->cur_khz = cur;

        snprintf(path, sizeof(path),
                 "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", i);
        c->max_khz = read_long_sysfs(path);

        snprintf(path, sizeof(path),
                 "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_min_freq", i);
        c->min_khz = read_long_sysfs(path);

        snprintf(path, sizeof(path),
                 "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor", i);
        FILE *gf = fopen(path, "r");
        if (gf) {
            fgets(c->governor, sizeof(c->governor), gf);
            c->governor[strcspn(c->governor, "\n")] = '\0';
            fclose(gf);
        } else {
            strncpy(c->governor, "unknown", sizeof(c->governor) - 1);
        }

        c->throttled = (c->max_khz > 0 &&
                        (float)c->cur_khz < (float)c->max_khz * THROTTLE_RATIO);
        count++;
    }

    return count;
}

/* ?? Results writer ????????????????????????????????????????????????????? */

static void write_results(int score, int scan_num, int sigs,
                           const temp_t *temps, int ntemps,
                           int throttled_cores, int ncores)
{
    FILE *f = fopen(RESULTS_FILE, "w");
    if (!f) return;

    time_t t = time(NULL);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", localtime(&t));

    const char *grade = score >= 85 ? "NOMINAL"
                      : score >= 65 ? "WARM"
                      : score >= 45 ? "HOT"
                      : "CRITICAL";

    fprintf(f, "{\n"
               "  \"daemon\": \"" DAEMON_NAME "\",\n"
               "  \"timestamp\": \"%s\",\n"
               "  \"scan_number\": %d,\n"
               "  \"signals_fired\": %d,\n"
               "  \"thermal_score\": %d,\n"
               "  \"grade\": \"%s\",\n"
               "  \"throttled_cores\": %d,\n"
               "  \"total_cores\": %d,\n"
               "  \"temperatures\": [\n",
               ts, scan_num, sigs, score, grade, throttled_cores, ncores);

    for (int i = 0; i < ntemps; i++) {
        fprintf(f, "    {\"name\":\"%s\",\"type\":%d,"
                   "\"cached\":%.1f,\"hal\":%.1f,\"delta\":%.1f}%s\n",
                temps[i].name, temps[i].type,
                temps[i].cached, temps[i].hal, temps[i].delta,
                i < ntemps - 1 ? "," : "");
    }

    fprintf(f, "  ]\n}\n");
    fclose(f);
}

/* ?? Main poll ?????????????????????????????????????????????????????????? */

static void poll(int scan_num)
{
    char ts[32];
    time_t t = time(NULL);
    strftime(ts, sizeof(ts), "%H:%M:%S", localtime(&t));

    printf("\n[LEATHER] ?? Thermal Truth Scan #%d  %s ?????????????????\n",
           scan_num, ts);

    int score = 100;
    int sigs  = 0;

    /* ?? Thermal service ???????????????????????????????????????????????? */
    char *dump = bexec("dumpsys thermalservice 2>/dev/null");

    temp_t temps[MAX_TEMP_TYPES];
    int ntemps = parse_thermalservice(dump, temps, MAX_TEMP_TYPES);
    int tstatus = parse_thermal_status(dump);

    printf("[LEATHER]  %-10s  %8s  %8s  %8s  %s\n",
           "Sensor", "CacheddegC", "HALdegC", "Delta", "Status");
    printf("[LEATHER]  ??????????????????????????????????????????????????\n");

    float skin_hal    = -1.0f;
    float battery_hal = -1.0f;
    bool  mismatch_fired = false;

    for (int i = 0; i < ntemps; i++) {
        temp_t *tp = &temps[i];
        float delta_abs = tp->delta < 0 ? -tp->delta : tp->delta;

        const char *flag = "";
        if (delta_abs >= MISMATCH_THRESHOLD) flag = " ! MISMATCH";

        printf("[LEATHER]  %-10s  %8.1f  %8.1f  %+7.1f%s\n",
               tp->name, tp->cached, tp->hal, tp->delta, flag);

        /* Track skin and battery */
        if (strcasecmp(tp->name, "SKIN") == 0)    skin_hal    = tp->hal > 0 ? tp->hal : tp->cached;
        if (strcasecmp(tp->name, "BATTERY") == 0) battery_hal = tp->hal > 0 ? tp->hal : tp->cached;

        /* HAL mismatch signal */
        if (delta_abs >= MISMATCH_THRESHOLD && !mismatch_fired) {
            char ctx[128];
            snprintf(ctx, sizeof(ctx),
                     "sensor=%.12s cached=%.1f hal=%.1f delta=%.1f",
                     tp->name, tp->cached, tp->hal, tp->delta);
            gaveld_emit(DAEMON_NAME, "THERMAL_HAL_MISMATCH", delta_abs, ctx);
            splinterd_emit("THERMAL_HAL_MISMATCH", ctx);
            score -= 15;
            sigs++;
            mismatch_fired = true;
        }
    }

    printf("[LEATHER]  ??????????????????????????????????????????????????\n");

    /* Thermal status */
    if (tstatus > 0) {
        char ctx[32];
        snprintf(ctx, sizeof(ctx), "status=%d", tstatus);
        gaveld_emit(DAEMON_NAME, "THERMAL_STATUS_NONZERO", (float)tstatus, ctx);
        splinterd_emit("THERMAL_STATUS_NONZERO", ctx);
        score -= tstatus * 10;
        sigs++;
        printf("[LEATHER]  Status     : !  Android thermal status = %d\n", tstatus);
    } else {
        printf("[LEATHER]  Status     : OK (0)\n");
    }

    /* Skin temp signals */
    if (skin_hal > 0) {
        if (skin_hal >= SKIN_CRITICAL_C) {
            char ctx[48];
            snprintf(ctx, sizeof(ctx), "skin_temp=%.1f", skin_hal);
            gaveld_emit(DAEMON_NAME, "THERMAL_CRITICAL", skin_hal, ctx);
            splinterd_emit("THERMAL_CRITICAL", ctx);
            score -= 25;
            sigs++;
            printf("[LEATHER]  Skin       : !!  CRITICAL %.1fdegC\n", skin_hal);
        } else if (skin_hal >= SKIN_WARN_C) {
            char ctx[48];
            snprintf(ctx, sizeof(ctx), "skin_temp=%.1f", skin_hal);
            gaveld_emit(DAEMON_NAME, "THERMAL_WARN", skin_hal, ctx);
            splinterd_emit("THERMAL_WARN", ctx);
            score -= 12;
            sigs++;
            printf("[LEATHER]  Skin       : !  WARM %.1fdegC\n", skin_hal);
        } else {
            printf("[LEATHER]  Skin       : %.1fdegC ok\n", skin_hal);
        }
    }

    /* Battery temp */
    if (battery_hal >= BATTERY_WARN_C) {
        char ctx[48];
        snprintf(ctx, sizeof(ctx), "battery_temp=%.1f", battery_hal);
        gaveld_emit(DAEMON_NAME, "BATTERY_TEMP_HIGH", battery_hal, ctx);
        splinterd_emit("BATTERY_TEMP_HIGH", ctx);
        score -= 15;
        sigs++;
        printf("[LEATHER]  Battery    : !  %.1fdegC\n", battery_hal);
    } else if (battery_hal > 0) {
        printf("[LEATHER]  Battery    : %.1fdegC ok\n", battery_hal);
    }

    if (dump) free(dump);

    /* ?? CPU cores ?????????????????????????????????????????????????????? */
    cpu_core_t cores[MAX_CPU_CORES];
    int ncores = enumerate_cpus(cores, MAX_CPU_CORES);

    printf("\n[LEATHER]  CPU Cores (%d found)\n", ncores);
    printf("[LEATHER]  %-5s  %-10s  %-10s  %-10s  %-12s  %s\n",
           "Core", "Cur MHz", "Max MHz", "Min MHz", "Governor", "State");
    printf("[LEATHER]  ?????????????????????????????????????????????????\n");

    int throttled_count = 0;
    int maxed_count     = 0;

    for (int i = 0; i < ncores; i++) {
        cpu_core_t *c = &cores[i];
        const char *state = c->throttled ? "THROTTLED" : "OK";
        if (c->throttled) throttled_count++;
        if (c->max_khz > 0 && c->cur_khz >= c->max_khz) maxed_count++;

        printf("[LEATHER]  cpu%-2d  %-10ld  %-10ld  %-10ld  %-12s  %s\n",
               c->core,
               c->cur_khz / 1000,
               c->max_khz / 1000,
               c->min_khz / 1000,
               c->governor,
               state);
    }

    /* Throttling signals */
    if (throttled_count > 0) {
        char ctx[64];
        snprintf(ctx, sizeof(ctx), "throttled=%d total=%d",
                 throttled_count, ncores);
        gaveld_emit(DAEMON_NAME, "CPU_THROTTLING", (float)throttled_count, ctx);
        splinterd_emit("CPU_THROTTLING", ctx);
        score -= 8 * throttled_count;
        sigs++;
        printf("[LEATHER]  !  %d/%d cores throttled\n", throttled_count, ncores);
    }

    /* All cores maxed */
    if (ncores > 0 && maxed_count == ncores) {
        char ctx[32];
        snprintf(ctx, sizeof(ctx), "cores=%d", ncores);
        gaveld_emit(DAEMON_NAME, "ALL_CORES_MAXED", (float)ncores, ctx);
        splinterd_emit("ALL_CORES_MAXED", ctx);
        score -= 15;
        sigs++;
        printf("[LEATHER]  !  All %d cores at max frequency\n", ncores);
    }

    if (score < 0) score = 0;

    const char *grade = score >= 85 ? "NOMINAL"
                      : score >= 65 ? "WARM"
                      : score >= 45 ? "HOT"
                      : "CRITICAL";

    printf("\n[LEATHER]  Thermal score : %d/100  [%s]  signals=%d\n",
           score, grade, sigs);

    write_results(score, scan_num, sigs, temps, ntemps,
                  throttled_count, ncores);
}

/* ?? Main ??????????????????????????????????????????????????????????????? */

int main(void)
{
    bexec_init();

    if (!is_enabled()) {
        printf("[LEATHER] disabled via syndicatectl -- exiting\n");
        return 0;
    }

    printf("[LEATHER] Thermal Truth & HAL Integrity Daemon: ONLINE\n");
    printf("[LEATHER] HAL mismatch threshold: %.1fdegC\n", MISMATCH_THRESHOLD);
    printf("[LEATHER] Skin warn=%.1fdegC  critical=%.1fdegC\n",
           SKIN_WARN_C, SKIN_CRITICAL_C);

    int interval  = get_interval();
    int max_scans = get_max_scans();
    int scan_num  = 0;

    for (;;) {
        if (!is_enabled()) {
            printf("[LEATHER] disabled -- stopping\n");
            break;
        }

        interval  = get_interval();
        max_scans = get_max_scans();
        scan_num++;

        poll(scan_num);

        if (max_scans > 0 && scan_num >= max_scans) {
            printf("[LEATHER] reached scan_count=%d -- exiting\n", max_scans);
            break;
        }

        printf("[LEATHER] Next scan in %ds\n", interval);
        sleep(interval);
    }

    return 0;
}
