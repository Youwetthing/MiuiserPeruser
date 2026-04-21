#include "splinter_selector.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static char* run_cmd(const char *cmd) {
    FILE *f = popen(cmd, "r");
    if (!f) return NULL;
    char *buf = malloc(256);
    if (!buf) { pclose(f); return NULL; }
    if (!fgets(buf, 256, f)) {
        free(buf);
        pclose(f);
        return NULL;
    }
    buf[strcspn(buf, "\n")] = 0;
    pclose(f);
    return buf;
}

backend_kind_t splinter_pick_backend(void) {
    /* Real checks — no stubs */
    char *sysfs_thermal = run_cmd("cat /sys/class/thermal/thermal_zone*/*temp 2>/dev/null | head -1");
    char *rish_test     = run_cmd("rish -c 'echo rish_ok' 2>/dev/null");
    char *adb_test      = run_cmd("getprop init.svc.adbd 2>/dev/null");
    char *thermal_svc   = run_cmd("dumpsys thermalservice | grep -q 'Thermal Status' && echo thermalservice_ok");
    char *portbridge    = run_cmd("getprop persist.sys.portbridge 2>/dev/null");

    if (sysfs_thermal && strlen(sysfs_thermal) > 0) {
        free(sysfs_thermal); free(rish_test); free(adb_test); free(thermal_svc); free(portbridge);
        return BACKEND_SYSFS;
    }
    if (rish_test && strstr(rish_test, "rish_ok")) {
        free(sysfs_thermal); free(rish_test); free(adb_test); free(thermal_svc); free(portbridge);
        return BACKEND_RISH;
    }
    if (adb_test && strstr(adb_test, "running") && thermal_svc) {
        free(sysfs_thermal); free(rish_test); free(adb_test); free(thermal_svc); free(portbridge);
        return BACKEND_ADB;
    }
    if (portbridge && strlen(portbridge) > 0) {
        free(sysfs_thermal); free(rish_test); free(adb_test); free(thermal_svc); free(portbridge);
        return BACKEND_PORTBRIDGE;
    }

    free(sysfs_thermal); free(rish_test); free(adb_test); free(thermal_svc); free(portbridge);
    return BACKEND_NONE;
}

const char *splinter_backend_name(backend_kind_t b) {
    switch (b) {
        case BACKEND_SYSFS:      return "sysfs";
        case BACKEND_RISH:       return "rish";
        case BACKEND_ADB:        return "adb";
        case BACKEND_PORTBRIDGE: return "portbridge";
        default:                 return "none";
    }
}
