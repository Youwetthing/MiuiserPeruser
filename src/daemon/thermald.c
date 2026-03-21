#include "fugitoid_log.h"
#include "fugitoid_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include "krang.h"
#include "fugitoid_log.h"

#define THERMAL_PATH "/sys/class/thermal"
#define MAX_ZONE_NAME 64
#define MAX_PATH 256

typedef struct {
    char name[MAX_ZONE_NAME];
    int temp_mC; // milli-Celsius
} thermal_zone_t;

static const char *classify_temp(int temp_mC) {
    if (temp_mC < 35000)  return "cool";
    if (temp_mC < 45000)  return "warm";
    if (temp_mC < 60000)  return "hot";
    return "critical";
}

static const char *guess_zone_role(const char *name) {
    if (!name) return "unknown";

    if (strstr(name, "cpu"))      return "CPU";
    if (strstr(name, "gpu"))      return "GPU";
    if (strstr(name, "battery"))  return "Battery";
    if (strstr(name, "batt"))     return "Battery";
    if (strstr(name, "skin"))     return "Skin";
    if (strstr(name, "xo_therm")) return "Modem";
    if (strstr(name, "modem"))    return "Modem";

    return "Other";
}

static int read_zone_temp(const char *zone_path, thermal_zone_t *out) {
    char type_path[MAX_PATH];
    char temp_path[MAX_PATH];
    FILE *f;
    char buf[128];

    snprintf(type_path, sizeof(type_path), "%s/type", zone_path);
    snprintf(temp_path, sizeof(temp_path), "%s/temp", zone_path);

    f = fopen(type_path, "r");
    if (!f) return -1;
    if (!fgets(out->name, sizeof(out->name), f)) {
        fclose(f);
        return -1;
    }
    fclose(f);

    // strip newline
    size_t len = strlen(out->name);
    if (len > 0 && out->name[len - 1] == '\n')
        out->name[len - 1] = '\0';

    f = fopen(temp_path, "r");
    if (!f) return -1;
    if (!fgets(buf, sizeof(buf), f)) {
        fclose(f);
        return -1;
    }
    fclose(f);

    out->temp_mC = atoi(buf);
    return 0;
}

static void scan_thermal_zones(void) {
    DIR *dir = opendir(THERMAL_PATH);
    if (!dir) {
        fugitoid_log("ERROR", "[LEATHERHEAD] Failed to open %s: %s",
                    THERMAL_PATH, strerror(errno));
        return;
    }

    struct dirent *de;
    char zone_path[MAX_PATH];
    int zones_found = 0;

    fugitoid_log("INFO", "[LEATHERHEAD] Scanning thermal zones...");

    while ((de = readdir(dir)) != NULL) {
        if (strncmp(de->d_name, "thermal_zone", 12) != 0)
            continue;

        snprintf(zone_path, sizeof(zone_path), "%s/%s", THERMAL_PATH, de->d_name);

        thermal_zone_t zone;
        if (read_zone_temp(zone_path, &zone) == 0) {
            const char *role = guess_zone_role(zone.name);
            const char *state = classify_temp(zone.temp_mC);

            fugitoid_log("INFO",
                        "[LEATHERHEAD] Zone '%s' (%s): %d mC (%s)",
                        zone.name, role, zone.temp_mC, state);

            zones_found++;
        }
    }

    closedir(dir);

    if (zones_found == 0) {
        fugitoid_log("WARN", "[LEATHERHEAD] No thermal zones found under %s", THERMAL_PATH);
    }
}

static void leatherhead_heartbeat(void) {
    krang_send_command("echo thermald_heartbeat");
    fugitoid_log("INFO", "[LEATHERHEAD] Heartbeat sent via Krang");
}

int main(void) {
    fugitoid_init("thermald");

    fugitoid_log("INFO", "[LEATHERHEAD] thermald starting up...");

    int hb_counter = 0;

    for (;;) {
        scan_thermal_zones();

        hb_counter++;
        if (hb_counter >= 360) { // ~3 hours if we sleep 30s
            leatherhead_heartbeat();
            hb_counter = 0;
        }

        sleep(30);
    }

    return 0;
}
