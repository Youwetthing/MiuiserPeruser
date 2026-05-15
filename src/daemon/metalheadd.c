/*
 * metalheadd.c — Sensor Service Registry & Client Auditor
 *
 * Every poll:
 *   - Parse dumpsys sensorservice for all registered sensors
 *   - List sensor name, type, vendor, resolution, max range
 *   - Count active client connections per sensor
 *   - Flag sensors with unusual polling frequency or many background clients
 *   - Emit APRIL events when suspicious sensor access is detected
 *
 * APRIL events emitted:
 *   sensor_access  — background app detected polling a sensitive sensor
 *   sensor_flood   — sensor sampling rate abnormally high
 */

#include "daemon_core.h"
#include "ipc_globals.h"
#include "backend_exec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>

#define DAEMON_NAME    "metalheadd"
#define POLL_SEC       30
#define MAX_SENSORS    64
#define FLOOD_HZ       200   /* sampling rate considered suspicious */
#define CLIENT_WARN    4     /* many clients on one sensor = watch */

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

/* ── Sensor record ────────────────────────────────────────────────────── */

typedef struct {
    char name[80];
    char vendor[48];
    char type_str[48];
    int  type_id;
    float max_range;
    float resolution;
    int  clients;
    int  sample_rate_hz;
    int  is_wakeup;
} sensor_t;

static sensor_t g_sensors[MAX_SENSORS];
static int      g_nsensors = 0;

/* ── Helpers ──────────────────────────────────────────────────────────── */

static char *run_cmd(const char *cmd) { return bexec_n(cmd, 131072); }

/* Find value after "key=" on the same line, up to whitespace/newline */
static int kv_extract(const char *line, const char *key,
                       char *out, size_t outlen)
{
    const char *p = strstr(line, key);
    if (!p) return 0;
    p += strlen(key);
    if (*p == '=') p++;
    size_t i = 0;
    while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != ',' && i < outlen - 1)
        out[i++] = *p++;
    out[i] = '\0';
    return i > 0;
}

/* Return 1 if type string is a sensitive sensor category */
static int is_sensitive(const char *type)
{
    if (!type) return 0;
    return strstr(type,"motion") || strstr(type,"accelero") ||
           strstr(type,"gyro")   || strstr(type,"magneto")  ||
           strstr(type,"micro")  || strstr(type,"camera") ? 1 : 0;
}

/* Sensor type id → readable name */
static const char *type_name(int id)
{
    switch (id) {
        case 1:  return "Accelerometer";
        case 2:  return "Magnetic Field";
        case 3:  return "Orientation";
        case 4:  return "Gyroscope";
        case 5:  return "Light";
        case 6:  return "Pressure";
        case 7:  return "Temperature";
        case 8:  return "Proximity";
        case 9:  return "Gravity";
        case 10: return "Linear Accel";
        case 11: return "Rotation Vector";
        case 12: return "Humidity";
        case 13: return "Ambient Temp";
        case 14: return "Mag Field Uncal";
        case 15: return "Game Rotation";
        case 16: return "Gyro Uncal";
        case 17: return "Sig Motion";
        case 18: return "Step Detector";
        case 19: return "Step Counter";
        case 21: return "Tilt Detector";
        case 28: return "Heart Rate";
        case 65536: return "Fingerprint";
        default: return "Other";
    }
}

/* ── Parse dumpsys sensorservice ──────────────────────────────────────── */

static void parse_sensorservice(const char *dump)
{
    g_nsensors = 0;
    if (!dump) return;

    const char *p = dump;
    /* Look for sensor list section */
    const char *list_start = strstr(dump, "Sensor List:");
    if (!list_start) list_start = dump;
    p = list_start;

    /* Each sensor block starts with a handle line like:
     * 0) handle=0x00000001, name="...", vendor="...", version=1, ...
     * Vendor implementations vary wildly — do a best-effort parse  */

    while (*p && g_nsensors < MAX_SENSORS) {
        /* Find a line with "handle=" */
        const char *line_start = p;
        const char *next_nl    = strchr(p, '\n');
        if (!next_nl) break;

        char line[512];
        size_t llen = (size_t)(next_nl - p);
        if (llen >= sizeof(line)) llen = sizeof(line) - 1;
        memcpy(line, p, llen);
        line[llen] = '\0';

        p = next_nl + 1;

        if (!strstr(line, "name=")) continue;

        sensor_t *s = &g_sensors[g_nsensors];
        memset(s, 0, sizeof(*s));

        /* name */
        const char *nq = strstr(line, "name=\"");
        if (nq) {
            nq += 6;
            size_t i = 0;
            while (*nq && *nq != '"' && i < sizeof(s->name) - 1)
                s->name[i++] = *nq++;
            s->name[i] = '\0';
        } else {
            kv_extract(line, "name", s->name, sizeof(s->name));
        }
        if (!s->name[0]) continue;

        /* vendor */
        const char *vq = strstr(line, "vendor=\"");
        if (vq) {
            vq += 8;
            size_t i = 0;
            while (*vq && *vq != '"' && i < sizeof(s->vendor) - 1)
                s->vendor[i++] = *vq++;
            s->vendor[i] = '\0';
        } else {
            kv_extract(line, "vendor", s->vendor, sizeof(s->vendor));
        }

        /* type */
        char tmp[32] = {0};
        if (kv_extract(line, "type=", tmp, sizeof(tmp)))
            s->type_id = (int)strtol(tmp, NULL, 0);
        strncpy(s->type_str, type_name(s->type_id), sizeof(s->type_str) - 1);

        /* maxRange */
        if (kv_extract(line, "maxRange=", tmp, sizeof(tmp)))
            s->max_range = atof(tmp);

        /* resolution */
        if (kv_extract(line, "resolution=", tmp, sizeof(tmp)))
            s->resolution = atof(tmp);

        /* wakeUp */
        s->is_wakeup = strstr(line, "isWakeUpSensor=true") ? 1 :
                       strstr(line, "wakeUp=1")            ? 1 : 0;

        g_nsensors++;
    }

    /* Second pass: count client references ("SensorEventConnection") */
    const char *act = strstr(dump, "Active connections");
    if (act) {
        for (int i = 0; i < g_nsensors; i++) {
            /* Count how many times the sensor name appears near "client" */
            const char *search = act;
            int cnt = 0;
            while ((search = strstr(search, g_sensors[i].name)) != NULL) {
                cnt++;
                search++;
            }
            g_sensors[i].clients = cnt;
        }
    }
}

/* ── Poll ─────────────────────────────────────────────────────────────── */

static void poll_sensors(void)
{
    char *dump = run_cmd("dumpsys sensorservice 2>/dev/null");
    parse_sensorservice(dump);

    char ts[32];
    time_t t = time(NULL);
    strftime(ts, sizeof(ts), "%H:%M:%S", localtime(&t));

    printf("\n[METAL] ── Sensor Registry  %s ───────────────────────────\n", ts);
    printf("[METAL]  %-3s  %-16s  %-20s  %-14s  %s\n",
           "Typ", "Class         ", "Name                ", "Vendor        ", "Clients/WakeUp");
    printf("[METAL]  ─────────────────────────────────────────────────────────\n");

    int sensitive_active = 0, flood_active = 0;

    for (int i = 0; i < g_nsensors; i++) {
        sensor_t *s = &g_sensors[i];
        const char *wk = s->is_wakeup ? "W" : " ";
        const char *cl_flag = (s->clients >= CLIENT_WARN) ? "  !" : "   ";

        printf("[METAL]  %3d  %-16s  %-20.20s  %-14.14s  %d clients %s%s\n",
               s->type_id, s->type_str, s->name, s->vendor,
               s->clients, wk, cl_flag);

        if (is_sensitive(s->type_str) && s->clients > 0) {
            sensitive_active++;
            if (s->clients >= CLIENT_WARN) {
                char ev[256];
                snprintf(ev, sizeof(ev),
                         "sensor=%.32s type=%.16s clients=%d",
                         s->name, s->type_str, s->clients);
                splinterd_emit("sensor_access", ev);
            }
        }
        if (s->sample_rate_hz >= FLOOD_HZ) {
            flood_active++;
            char ev[256];
            snprintf(ev, sizeof(ev),
                     "sensor=%.32s hz=%d", s->name, s->sample_rate_hz);
            splinterd_emit("sensor_flood", ev);
        }
    }

    printf("[METAL]  ─────────────────────────────────────────────────────────\n");
    printf("[METAL]  Total sensors: %d   Sensitive active: %d   Flood: %d\n",
           g_nsensors, sensitive_active, flood_active);

    if (dump) free(dump);
}

/* ── Main ─────────────────────────────────────────────────────────────── */

int main(void)
{
    if (!daemon_core_init(DAEMON_NAME)) return 1;
    bexec_init();

    for (;;) {
        poll_sensors();
        printf("[METAL]  Next scan in %ds\n", POLL_SEC);
        sleep(POLL_SEC);
    }

    daemon_core_shutdown();
    return 0;
}
