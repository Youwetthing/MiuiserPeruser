#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "backend_sysinfo.h"

static void read_first_line(const char *path, char *out, size_t outlen)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        snprintf(out, outlen, "unknown");
        return;
    }

    if (!fgets(out, outlen, f)) {
        snprintf(out, outlen, "unknown");
        fclose(f);
        return;
    }

    /* strip newline */
    out[strcspn(out, "\n")] = '\0';
    fclose(f);
}

char *backend_sysinfo(void)
{
    struct utsname uts;
    uname(&uts);

    char android_ver[64];
    char device_model[128];
    char uptime_buf[64];
    char loadavg_buf[64];
    char meminfo_buf[128];

    read_first_line("/system/build.prop", android_ver, sizeof(android_ver));
    read_first_line("/proc/device-tree/model", device_model, sizeof(device_model));
    read_first_line("/proc/uptime", uptime_buf, sizeof(uptime_buf));
    read_first_line("/proc/loadavg", loadavg_buf, sizeof(loadavg_buf));
    read_first_line("/proc/meminfo", meminfo_buf, sizeof(meminfo_buf));

    char *out = malloc(512);
    if (!out)
        return strdup("sysinfo:error");

    snprintf(out, 512,
             "kernel=%s android=%s model=%s uptime=%s load=%s mem=%s",
             uts.release,
             android_ver,
             device_model,
             uptime_buf,
             loadavg_buf,
             meminfo_buf);

    return out;
}
