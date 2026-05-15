/*
 * leatherheadd.c — Full-Spectrum Thermal Warden
 *
 * Every poll:
 *   - Walk /sys/class/thermal/thermal_zone* — read type + temp for every zone
 *   - Classify zones: CPU, GPU, battery, skin, board, unknown
 *   - Track highest reading per class and overall peak
 *   - Detect heating trends (delta between successive polls)
 *   - Emit APRIL events to splinterd at warning / critical thresholds
 *
 * APRIL events emitted:
 *   thermal_warn     — any zone >= WARN_C
 *   thermal_critical — any zone >= CRIT_C
 *   thermal_trend    — rapid temperature rise > TREND_C in one interval
 */

#include "daemon_core.h"
#include "ipc_globals.h"
#include "backend_exec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/un.h>

#define DAEMON_NAME  "leatherheadd"
#define POLL_SEC     10
#define WARN_C       38.0f
#define CRIT_C       45.0f
#define TREND_C       5.0f   /* °C rise per poll = rapid heat */
#define MAX_ZONES    64

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

/* ── Zone classification ──────────────────────────────────────────────── */

typedef enum {
    ZONE_CPU = 0, ZONE_GPU, ZONE_BATTERY, ZONE_SKIN,
    ZONE_BOARD, ZONE_CHARGER, ZONE_UNKNOWN
} zone_class_t;

static const char *zone_class_name(zone_class_t c) {
    switch (c) {
        case ZONE_CPU:     return "CPU    ";
        case ZONE_GPU:     return "GPU    ";
        case ZONE_BATTERY: return "Battery";
        case ZONE_SKIN:    return "Skin   ";
        case ZONE_BOARD:   return "Board  ";
        case ZONE_CHARGER: return "Charger";
        default:           return "Other  ";
    }
}

static zone_class_t classify(const char *type)
{
    if (!type) return ZONE_UNKNOWN;
    /* lower-case compare via strstr — type strings vary by vendor */
    if (strstr(type,"cpu") || strstr(type,"CPU") || strstr(type,"tsens"))
        return ZONE_CPU;
    if (strstr(type,"gpu") || strstr(type,"GPU") || strstr(type,"kgsl"))
        return ZONE_GPU;
    if (strstr(type,"battery") || strstr(type,"BATTERY") || strstr(type,"batt"))
        return ZONE_BATTERY;
    if (strstr(type,"skin") || strstr(type,"SKIN") || strstr(type,"xo_therm"))
        return ZONE_SKIN;
    if (strstr(type,"chg") || strstr(type,"charger") || strstr(type,"CHARGER"))
        return ZONE_CHARGER;
    if (strstr(type,"board") || strstr(type,"pcb") || strstr(type,"BOARD"))
        return ZONE_BOARD;
    return ZONE_UNKNOWN;
}

/* ── Zone record ──────────────────────────────────────────────────────── */

typedef struct {
    char         type[64];
    int          id;
    float        temp_c;
    float        prev_c;
    zone_class_t cls;
} zone_t;

static zone_t g_zones[MAX_ZONES];
static int    g_nzones = 0;
static int    g_first  = 1;

/* ── Read one sysfs float via backend (millidegrees → deg C) ─────────── */

static float read_temp_file(const char *path)
{
    /* bexec_read_file tries fopen first, falls back to privileged cat */
    char *raw_str = bexec_read_file(path);
    if (!raw_str) return -999.0f;
    long raw = atol(raw_str);
    free(raw_str);
    return (raw > 1000) ? (float)raw / 1000.0f : (float)raw;
}

/* ── Discovery + poll ─────────────────────────────────────────────────── */

static void discover_zones(void)
{
    g_nzones = 0;
    DIR *d = opendir("/sys/class/thermal");
    if (!d) return;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && g_nzones < MAX_ZONES) {
        if (strncmp(ent->d_name, "thermal_zone", 12) != 0) continue;
        int id = atoi(ent->d_name + 12);
        char path[128];

        /* Read type */
        snprintf(path, sizeof(path),
                 "/sys/class/thermal/%s/type", ent->d_name);
        FILE *tf = fopen(path, "r");
        if (!tf) continue;
        char type[64] = {0};
        fgets(type, sizeof(type), tf);
        fclose(tf);
        type[strcspn(type, "\n")] = '\0';

        zone_t *z       = &g_zones[g_nzones++];
        z->id           = id;
        z->prev_c       = -999.0f;
        z->temp_c       = -999.0f;
        z->cls          = classify(type);
        strncpy(z->type, type, sizeof(z->type) - 1);
    }
    closedir(d);
}

static void poll_thermals(void)
{
    float peak_c    = -999.0f;
    char  peak_name[64] = "none";
    int   warn_cnt  = 0;
    int   crit_cnt  = 0;
    float class_max[7] = { -999, -999, -999, -999, -999, -999, -999 };
    char  class_name_max[7][64];
    memset(class_name_max, 0, sizeof(class_name_max));

    char ts[32];
    time_t t = time(NULL);
    strftime(ts, sizeof(ts), "%H:%M:%S", localtime(&t));

    printf("\n[LEATHER] ── Thermal Map  %s ──────────────────────────\n", ts);
    printf("[LEATHER]  %-4s  %-7s  %-28s  %s\n",
           "Zone", "Class  ", "Type", "Temp(°C)");
    printf("[LEATHER]  ───────────────────────────────────────────────\n");

    for (int i = 0; i < g_nzones; i++) {
        zone_t *z = &g_zones[i];
        char path[128];
        snprintf(path, sizeof(path),
                 "/sys/class/thermal/thermal_zone%d/temp", z->id);
        z->prev_c = (g_first) ? -999.0f : z->temp_c;
        z->temp_c = read_temp_file(path);
        if (z->temp_c <= -900.0f) continue;   /* unreadable zone */

        float delta = (z->prev_c > -900.0f) ? (z->temp_c - z->prev_c) : 0.0f;

        /* Status indicator */
        const char *ind = "  ok";
        if      (z->temp_c >= CRIT_C)          ind = "  CRITICAL";
        else if (z->temp_c >= WARN_C)           ind = "  WARNING";
        else if (!g_first && delta >= TREND_C)  ind = "  RISING";

        printf("[LEATHER]  z%-3d  %s  %-28s  %6.1f  (%+.1f)%s\n",
               z->id, zone_class_name(z->cls), z->type,
               z->temp_c, delta, ind);

        /* Track peak */
        if (z->temp_c > peak_c) {
            peak_c = z->temp_c;
            strncpy(peak_name, z->type, sizeof(peak_name) - 1);
        }
        /* Per-class max */
        if (z->temp_c > class_max[z->cls]) {
            class_max[z->cls] = z->temp_c;
            strncpy(class_name_max[z->cls], z->type,
                    sizeof(class_name_max[z->cls]) - 1);
        }

        if (z->temp_c >= CRIT_C) crit_cnt++;
        else if (z->temp_c >= WARN_C) warn_cnt++;
    }

    printf("[LEATHER]  ───────────────────────────────────────────────\n");
    printf("[LEATHER]  Peak: %.1f°C  (%s)   "
           "Warn zones: %d   Critical: %d\n",
           peak_c, peak_name, warn_cnt, crit_cnt);

    /* Class summary */
    float cpu_max = class_max[ZONE_CPU];
    float skin    = class_max[ZONE_SKIN];
    float batt    = class_max[ZONE_BATTERY];
    printf("[LEATHER]  CPU=%.1f°C  Skin=%.1f°C  Battery=%.1f°C\n",
           cpu_max  > -900 ? cpu_max : 0.0f,
           skin     > -900 ? skin    : 0.0f,
           batt     > -900 ? batt    : 0.0f);

    /* APRIL events */
    if (crit_cnt > 0) {
        char ev[256];
        snprintf(ev, sizeof(ev),
                 "peak_c=%.1f zone=%.32s critical_zones=%d warn_zones=%d",
                 peak_c, peak_name, crit_cnt, warn_cnt);
        splinterd_emit("thermal_critical", ev);
    } else if (warn_cnt > 0) {
        char ev[256];
        snprintf(ev, sizeof(ev),
                 "peak_c=%.1f zone=%.32s warn_zones=%d",
                 peak_c, peak_name, warn_cnt);
        splinterd_emit("thermal_warn", ev);
    }

    /* Trend: check any zone rising fast */
    for (int i = 0; i < g_nzones; i++) {
        zone_t *z = &g_zones[i];
        if (z->prev_c > -900.0f && (z->temp_c - z->prev_c) >= TREND_C) {
            char ev[256];
            snprintf(ev, sizeof(ev),
                     "zone=%.32s from_c=%.1f to_c=%.1f delta=%.1f",
                     z->type, z->prev_c, z->temp_c, z->temp_c - z->prev_c);
            splinterd_emit("thermal_trend", ev);
            break;
        }
    }

    g_first = 0;
}

/* ── Main ─────────────────────────────────────────────────────────────── */

int main(void)
{
    if (!daemon_core_init(DAEMON_NAME)) return 1;

    bexec_init();
    printf("[LEATHER] Discovering thermal zones in /sys/class/thermal/...\n");
    discover_zones();
    printf("[LEATHER] Found %d thermal zones\n", g_nzones);

    for (;;) {
        poll_thermals();
        sleep(POLL_SEC);
    }

    daemon_core_shutdown();
    return 0;
}
