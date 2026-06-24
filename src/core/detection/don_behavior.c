#include "compat/sensei_compat.h"
#include "sensei_types.h"
#include "rish_pipe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

extern void april_log(const char* level, const char* format, ...);
extern SENSEI_STATUS april_detection_list_append(SENSEI_DETECTION_LIST *list, const SENSEI_DETECTION *det);

static void add_det(SENSEI_DETECTION_LIST *r,
                    SENSEI_DETECTION_CLASS cls, SENSEI_EVENT_PRIORITY pri,
                    SENSEI_MITRE_TECHNIQUE mitre, int conf,
                    const char *type, const char *desc) {
    SENSEI_DETECTION det = {0};
    det.detection_class = cls; det.priority = pri;
    det.mitre_id = mitre; det.confidence = conf;
    strncpy(det.detection_type, type, SENSEI_MAX_DETECTION_TYPE - 1);
    strncpy(det.description,    desc, SENSEI_MAX_DESCRIPTION    - 1);
    april_detection_list_append(r, &det);
}

/* Read a single line from /proc/PID/file — local, no rish */
static char *proc_read(uint32_t pid, const char *file) {
    char path[128];
    snprintf(path, sizeof(path), "/proc/%u/%s", pid, file);
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    char *buf = malloc(512);
    if (!buf) { fclose(f); return NULL; }
    buf[0] = 0;
    fgets(buf, 512, f);
    fclose(f);
    /* strip newline */
    size_t l = strlen(buf);
    while (l > 0 && (buf[l-1] == '\n' || buf[l-1] == '\r')) buf[--l] = 0;
    if (!l) { free(buf); return NULL; }
    return buf;
}

/* Count entries in /proc/PID/fd */
static int count_fds(uint32_t pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%u/fd", pid);
    DIR *d = opendir(path);
    if (!d) return -1;
    int count = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] != '.') count++;
    }
    closedir(d);
    return count;
}

SENSEI_STATUS don_behavior_analyze(uint32_t pid, SENSEI_DETECTION_LIST *results) {
    if (!results || pid == 0) return SENSEI_STATUS_ERROR;

    char desc[SENSEI_MAX_DESCRIPTION];

    /* 1. FD count — local /proc read, no rish */
    int fds = count_fds(pid);
    if (fds > 500) {
        snprintf(desc, sizeof(desc),
            "PID %u has %d open file descriptors — possible fd leak or exfil", pid, fds);
        add_det(results, SENSEI_DETECTION_CLASS_BEHAVIOR, SENSEI_EVENT_PRIORITY_MEDIUM,
                SENSEI_MITRE_T1071, 70, "EXCESSIVE_FDS", desc);
    }

    /* 2. Process state — local /proc read */
    char *state = proc_read(pid, "status");
    if (state && strstr(state, "R (running)")) {
        /* Read cmdline for context */
        char cmdpath[128];
        snprintf(cmdpath, sizeof(cmdpath), "/proc/%u/cmdline", pid);
        FILE *cf = fopen(cmdpath, "r");
        if (cf) {
            char cmdline[128] = {0};
            fread(cmdline, 1, sizeof(cmdline)-1, cf);
            fclose(cf);
            /* Replace null bytes with spaces */
            for (int i = 0; i < (int)sizeof(cmdline)-1; i++)
                if (cmdline[i] == '\0' && i > 0) cmdline[i] = ' ';
            /* Flag non-system processes actively running */
            if (strlen(cmdline) > 3 &&
                !strstr(cmdline, "com.android") &&
                !strstr(cmdline, "com.google") &&
                !strstr(cmdline, "system") &&
                !strstr(cmdline, "kworker") &&
                !strstr(cmdline, "com.miui")) {
                snprintf(desc, sizeof(desc),
                    "PID %u (%s) actively running — unexpected wakeup", pid, cmdline);
                add_det(results, SENSEI_DETECTION_CLASS_BEHAVIOR, SENSEI_EVENT_PRIORITY_LOW,
                        SENSEI_MITRE_T1071, 50, "UNEXPECTED_ACTIVE_PROC", desc);
            }
        }
    }
    free(state);

    return SENSEI_STATUS_OK;
}
