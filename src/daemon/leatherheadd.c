/*
 * leatherheadd.c -> Thermal Truth Warden
 *
 * Domain: thermal reality on MTK HyperOS
 *   - Raw sensor readings: /sys/class/thermal/thermal_zone[N]/temp
 *   - HAL-reported values: dumpsys thermalservice (via rish)
 *   - HAL vs sensor delta (mismatch detection)
 *   - Throttling: performance cluster cur_freq vs max_freq
 *   - Skin temp, battery temp, status
 *
 * Signals emitted:
 *   THERMAL_HAL_MISMATCH  -> HAL skin > raw max by >5degC
 *   THERMAL_WARN          -> max raw temp > 42degC
 *   THERMAL_CRITICAL      -> max raw temp > 48degC
 *   CPU_THROTTLING        -> perf cluster running < 60% of max freq
 *   SKIN_TEMP_HIGH        -> skin zone > threshold
 *   BATTERY_TEMP_HIGH     -> battery zone > 40degC
 *   THERMAL_STATUS_NONZERO-> Android thermal status > 0
 *
 * IPC (turtlecom worker):
 *   CAPABILITY-        -> CAPABILITY THERMAL STATUS
 *                         CAPABILITY THERMAL RAW
 *                         CAPABILITY THERMAL HAL
 *   THERMAL STATUS     -> one-line summary
 *   THERMAL RAW        -> max raw zone temp in millidegrees
 *   THERMAL HAL        -> HAL skin temp (dumpsys thermalservice)
 */

#include "daemon_core.h"
#include "gaveld_emit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
volatile sig_atomic_t g_running = 1;

#define DAEMON_NAME "leatherheadd"
#define BUS_PATH    "/data/data/com.termux/files/home/MiuiserPeruser/pipes/turtlecom.sock"

/* Thermal thresholds (millidegrees Celsius) */
#define THRESH_WARN      42000
#define THRESH_CRITICAL  48000
#define THRESH_BATTERY   40000
#define THRESH_MISMATCH   5000   /* HAL vs raw delta that flags mismatch */

/* Throttle: perf cluster (cpu4-7) running < this fraction of max */
#define THROTTLE_RATIO   0.60f

/* Probe every ~10 seconds (100 - 100ms sleep ticks) */
#define PROBE_TICKS 100

/* -- IPC ---------------------------------------------------------------- */

static int connect_bus(void)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, BUS_PATH, sizeof(addr.sun_path) - 1);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    fcntl(fd, F_SETFL, O_NONBLOCK);
    return fd;
}

/* -- Splinterd ---------------------------------------------------------- */

#ifndef SPLINTER_SOCKET
#define SPLINTER_SOCKET "/data/data/com.termux/files/home/MiuiserPeruser/pipes/splinterd.sock"
#endif

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

/* -- Thermal zone scan -------------------------------------------------- */

typedef struct {
    int max_temp_mc;   /* millidegrees -> max across all zones */
    int zone_count;
    int skin_temp_mc;  /* best-guess skin zone, -1 if not found */
    int battery_temp_mc;
} raw_thermal_t;

/*
 * On MTK HyperOS we may see 100+ thermal zones.
 * We try to identify skin and battery zones by name.
 * For zones without a name file we just track the max.
 */
static raw_thermal_t scan_thermal_zones(void)
{
    raw_thermal_t r = { .max_temp_mc = 0, .zone_count = 0,
                        .skin_temp_mc = -1, .battery_temp_mc = -1 };

    for (int z = 0; z < 150; z++) {
        char temp_path[128], type_path[128], type_buf[64];
        snprintf(temp_path, sizeof(temp_path),
                 "/sys/class/thermal/thermal_zone%d/temp", z);
        snprintf(type_path, sizeof(type_path),
                 "/sys/class/thermal/thermal_zone%d/type", z);

        FILE *f = fopen(temp_path, "r");
        if (!f) break;   /* zones are sequential; first missing = end */

        int temp_mc = 0;
        if (fscanf(f, "%d", &temp_mc) != 1) { fclose(f); continue; }
        fclose(f);

        r.zone_count++;
        if (temp_mc > r.max_temp_mc) r.max_temp_mc = temp_mc;

        /* Read zone type for classification */
        type_buf[0] = '\0';
        FILE *tf = fopen(type_path, "r");
        if (tf) {
            fgets(type_buf, sizeof(type_buf), tf);
            fclose(tf);
            type_buf[strcspn(type_buf, "\n\r")] = '\0';
        }

        /* Identify skin zone (device-specific names) */
        if (r.skin_temp_mc < 0 &&
            (strstr(type_buf, "skin") || strstr(type_buf, "quiet") ||
             strstr(type_buf, "xo") || strstr(type_buf, "pa"))) {
            r.skin_temp_mc = temp_mc;
        }
        /* Identify battery zone */
        if (r.battery_temp_mc < 0 &&
            (strstr(type_buf, "battery") || strstr(type_buf, "batt"))) {
            r.battery_temp_mc = temp_mc;
        }
    }
    return r;
}

/* -- HAL thermal (dumpsys thermalservice) ------------------------------- */
/*
 * Parse the first "Temperature{..." entry for skin type.
 * Returns temp in millidegrees, -1 on parse failure.
 * Needs rish for privileged dumpsys on HyperOS.
 */
static int read_hal_skin_temp(void)
{
    const char *cmd =
        "/data/data/com.termux/files/home/.shizuku/rish -c "
        "'dumpsys thermalservice 2>/dev/null' "
        "| grep -i 'skin\\|quiet\\|xo' "
        "| grep 'Temperature{' | head -n1 "
        "| grep -oP 'value=\\K[0-9.]+' | head -n1";
    FILE *f = popen(cmd, "r");
    if (!f) return -1;
    char buf[32] = {0};
    fgets(buf, sizeof(buf), f);
    pclose(f);
    if (!buf[0]) return -1;
    float deg = atof(buf);
    return (int)(deg * 1000.0f);   /* convert degC -> millidegrees */
}

/* -- CPU throttle check (perf cluster cpu4-7) -------------------------- */
/*
 * Returns throttle ratio: 1.0 = full speed, <1.0 = throttled.
 * Uses cpuinfo_max_freq (hardware max) not scaling_max_freq.
 */
static float check_throttle_ratio(void)
{
    long cur_sum = 0, max_sum = 0;
    int  count   = 0;

    for (int cpu = 4; cpu <= 7; cpu++) {   /* performance cluster */
        char path[128];
        long val;
        FILE *f;

        snprintf(path, sizeof(path),
                 "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", cpu);
        f = fopen(path, "r");
        if (!f) continue;
        if (fscanf(f, "%ld", &val) == 1) cur_sum += val;
        fclose(f);

        snprintf(path, sizeof(path),
                 "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", cpu);
        f = fopen(path, "r");
        if (!f) continue;
        if (fscanf(f, "%ld", &val) == 1) max_sum += val;
        fclose(f);
        count++;
    }
    if (!count || !max_sum) return 1.0f;
    return (float)cur_sum / (float)max_sum;
}

/* -- Android thermal status --------------------------------------------- */

static int read_thermal_status(void)
{
    FILE *f = popen("getprop sys.thermal.status 2>/dev/null", "r");
    if (!f) return -1;
    char buf[16] = {0};
    fgets(buf, sizeof(buf), f);
    pclose(f);
    return atoi(buf);
}

/* -- Probe & emit ------------------------------------------------------- */

static void probe_and_emit(int ipc_fd, int on_command)
{
    raw_thermal_t raw = scan_thermal_zones();
    int hal_mc        = read_hal_skin_temp();
    float throttle    = check_throttle_ratio();
    int therm_status  = read_thermal_status();

    int max_c     = raw.max_temp_mc / 1000;
    int skin_c    = raw.skin_temp_mc  > 0 ? raw.skin_temp_mc  / 1000 : -1;
    int batt_c    = raw.battery_temp_mc > 0 ? raw.battery_temp_mc / 1000 : -1;
    int hal_c     = hal_mc > 0 ? hal_mc / 1000 : -1;

    /* Build summary string */
    char summary[256];
    snprintf(summary, sizeof(summary),
             "max=%ddegC skin=%sdegC batt=%sdegC hal=%sdegC throttle=%.0f%% zones=%d status=%d",
             max_c,
             skin_c >= 0 ? (char[8]){0} : "-",   /* placeholder */
             batt_c >= 0 ? "-" : "-",
             hal_c  >= 0 ? "-" : "-",
             throttle * 100.0f,
             raw.zone_count, therm_status);
    /* Rebuild with actual values */
    snprintf(summary, sizeof(summary),
             "max=%ddegC skin=%ddegC batt=%ddegC hal=%ddegC throttle=%.0f%% zones=%d status=%d",
             max_c, skin_c, batt_c, hal_c,
             throttle * 100.0f, raw.zone_count, therm_status);

    daemon_log_info("Thermal: %s", summary);

    /* IPC response if command-driven */
    if (on_command && ipc_fd >= 0) {
        dprintf(ipc_fd, "THERMAL STATUS %s\n", summary);
    }

    /* -- Signal evaluation --------------------------------------------- */

    if (raw.max_temp_mc >= THRESH_CRITICAL) {
        char ctx[128];
        snprintf(ctx, sizeof(ctx), "max=%ddegC threshold=48degC", max_c);
        gaveld_emit(DAEMON_NAME, "THERMAL_CRITICAL", (float)max_c, ctx);
        splinterd_emit("thermal_critical", ctx);
    } else if (raw.max_temp_mc >= THRESH_WARN) {
        char ctx[128];
        snprintf(ctx, sizeof(ctx), "max=%ddegC threshold=42degC", max_c);
        gaveld_emit(DAEMON_NAME, "THERMAL_WARN", (float)max_c, ctx);
        splinterd_emit("thermal_warn", ctx);
    }

    if (raw.skin_temp_mc > 0 && raw.skin_temp_mc >= THRESH_WARN) {
        char ctx[128];
        snprintf(ctx, sizeof(ctx), "skin=%ddegC", skin_c);
        gaveld_emit(DAEMON_NAME, "SKIN_TEMP_HIGH", (float)skin_c, ctx);
        splinterd_emit("skin_temp_high", ctx);
    }

    if (raw.battery_temp_mc > 0 && raw.battery_temp_mc >= THRESH_BATTERY) {
        char ctx[128];
        snprintf(ctx, sizeof(ctx), "batt=%ddegC threshold=40degC", batt_c);
        gaveld_emit(DAEMON_NAME, "BATTERY_TEMP_HIGH", (float)batt_c, ctx);
        splinterd_emit("battery_temp_high", ctx);
    }

    /* HAL mismatch: HAL > raw by more than threshold */
    if (hal_mc > 0 && raw.max_temp_mc > 0) {
        int delta = hal_mc - raw.max_temp_mc;
        if (delta > THRESH_MISMATCH) {
            char ctx[128];
            snprintf(ctx, sizeof(ctx),
                     "hal=%ddegC raw_max=%ddegC delta=%ddegC",
                     hal_c, max_c, delta / 1000);
            gaveld_emit(DAEMON_NAME, "THERMAL_HAL_MISMATCH", (float)(delta/1000), ctx);
            splinterd_emit("thermal_hal_mismatch", ctx);
        }
    }

    if (throttle < THROTTLE_RATIO) {
        char ctx[128];
        snprintf(ctx, sizeof(ctx),
                 "perf_cluster_ratio=%.0f%%", throttle * 100.0f);
        gaveld_emit(DAEMON_NAME, "CPU_THROTTLING", throttle * 100.0f, ctx);
        splinterd_emit("cpu_throttling", ctx);
    }

    if (therm_status > 0) {
        char ctx[64];
        snprintf(ctx, sizeof(ctx), "android_thermal_status=%d", therm_status);
        gaveld_emit(DAEMON_NAME, "THERMAL_STATUS_NONZERO", (float)therm_status, ctx);
        splinterd_emit("thermal_status_nonzero", ctx);
    }
}

/* -- Main --------------------------------------------------------------- */

int main(void)
{
    

    int fd = connect_bus();
    if (fd < 0) {
        daemon_log_error(DAEMON_NAME ": cannot connect to turtlecom -> exiting");
        return 1;
    }

    write(fd, "HELLO WORKER LEATHERHEAD\n", 25);
    daemon_log_info(DAEMON_NAME " ONLINE -> Thermal Truth Warden");

    char buf[512];
    int  tick = 0;

    for (;;) {
        usleep(100000);   /* 100ms tick */
        tick++;

        /* Periodic self-probe */
        if (tick >= PROBE_TICKS) {
            tick = 0;
            probe_and_emit(fd, 0);
        }

        /* IPC */
        int n = read(fd, buf, sizeof(buf) - 1);
        if (n <= 0) continue;
        buf[n] = '\0';

        if (strncmp(buf, "CAPABILITY-", 11) == 0) {
            write(fd, "CAPABILITY THERMAL STATUS\n", 26);
            write(fd, "CAPABILITY THERMAL RAW\n",    23);
            write(fd, "CAPABILITY THERMAL HAL\n",    23);
            continue;
        }

        if (strncmp(buf, "THERMAL STATUS", 14) == 0) {
            probe_and_emit(fd, 1);
            continue;
        }

        if (strncmp(buf, "THERMAL RAW", 11) == 0) {
            raw_thermal_t r = scan_thermal_zones();
            dprintf(fd, "THERMAL RAW max_mc=%d zones=%d\n",
                    r.max_temp_mc, r.zone_count);
            continue;
        }

        if (strncmp(buf, "THERMAL HAL", 11) == 0) {
            int h = read_hal_skin_temp();
            dprintf(fd, "THERMAL HAL skin_mc=%d\n", h);
            continue;
        }
    }

    return 0;
}
