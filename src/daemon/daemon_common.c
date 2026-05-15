#include "daemon_common.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>

/* ── Shared state ─────────────────────────────────────────────────────── */
int miui_flag_restricted = 0;
int thermal_state        = 0;

/* ── process_running ──────────────────────────────────────────────────── *
 * Walk /proc looking for a matching comm name.                            *
 * Returns 1 if found, 0 otherwise.                                        */
int process_running(const char *name)
{
    if (!name || !*name) return 0;
    DIR *proc = opendir("/proc");
    if (!proc) return 0;

    struct dirent *ent;
    char comm_path[64], comm[32];
    int found = 0;

    while ((ent = readdir(proc)) != NULL && !found) {
        /* Only look at numeric entries (PIDs) */
        int is_pid = 1;
        for (const char *p = ent->d_name; *p; p++) {
            if (*p < '0' || *p > '9') { is_pid = 0; break; }
        }
        if (!is_pid) continue;

        snprintf(comm_path, sizeof(comm_path), "/proc/%s/comm", ent->d_name);
        FILE *f = fopen(comm_path, "r");
        if (!f) continue;
        if (fgets(comm, sizeof(comm), f)) {
            /* strip trailing newline */
            comm[strcspn(comm, "\n")] = '\0';
            if (strcmp(comm, name) == 0)
                found = 1;
        }
        fclose(f);
    }
    closedir(proc);
    return found;
}

/* ── device_get_property ──────────────────────────────────────────────── *
 * Weak stub so daemons that reference this symbol link without needing    *
 * the full Android property layer.  Returns "" on non-Android hosts.      */
__attribute__((weak))
const char *device_get_property(const char *key)
{
    (void)key;
    return "";
}
