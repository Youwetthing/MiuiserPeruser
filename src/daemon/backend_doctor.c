#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "backend_doctor.h"
#include "backend_adb.h"

// Check if a sysfs path exists
static int check_sysfs(const char *path)
{
    return access(path, R_OK) == 0;
}

// Check if ADB is ready
static int check_adb(void)
{
    char *state = backend_adb_exec("get-state");
    if (!state) return 0;

    int ok = (strcmp(state, "device") == 0);
    free(state);
    return ok;
}

// Check if dumpsys thermalservice works
static int check_thermalservice(void)
{
    char *out = backend_adb_exec("dumpsys thermalservice");
    if (!out) return 0;

    int ok = strstr(out, "Temperature") != NULL;
    free(out);
    return ok;
}

char *backend_doctor(void)
{
    int sysfs_cpu = check_sysfs("/sys/devices/system/cpu");
    int sysfs_thermal = check_sysfs("/sys/class/thermal");

    int adb = check_adb();
    int thermalservice = adb ? check_thermalservice() : 0;

    const char *backend =
        sysfs_thermal ? "sysfs" :
        thermalservice ? "adb" :
        "none";

    char *out = malloc(256);
    if (!out) return strdup("doctor:error");

    snprintf(out, 256,
        "sysfs_cpu=%s "
        "sysfs_thermal=%s "
        "adb=%s "
        "thermalservice=%s "
        "backend=%s",

        sysfs_cpu ? "yes" : "no",
        sysfs_thermal ? "yes" : "no",
        adb ? "yes" : "no",
        thermalservice ? "yes" : "no",
        backend
    );

    return out;
}

int backend_doctor_status(char *buf, int len) {
    if (!buf || len < 10) return -1;
    // Simple healthy status return
    snprintf(buf, len, "HEALTHY");
    return 0;
}
