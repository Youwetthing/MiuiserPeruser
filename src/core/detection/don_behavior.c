#include "compat/sensei_compat.h"
#include "sensei_types.h"
#include "rish_pipe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern void april_log(const char* level, const char* format, ...);
extern SENSEI_STATUS april_detection_list_append(SENSEI_DETECTION_LIST *list, const SENSEI_DETECTION *det);

static char *rish_cmd(const char *cmd) {
    char full[2048];
    snprintf(full, sizeof(full),
        "RISH_APPLICATION_ID=com.termux "
        "/data/data/com.termux/files/home/Rish/rish -c '%s' 2>/dev/null", cmd);
    FILE *fp = popen(full, "r");
    if (!fp) return NULL;
    size_t sz = 4096; char *out = malloc(sz);
    if (!out) { pclose(fp); return NULL; }
    out[0] = 0; size_t pos = 0; char buf[256];
    while (fgets(buf, sizeof(buf), fp)) {
        size_t len = strlen(buf);
        if (pos + len + 1 >= sz) { sz *= 2; char *t = realloc(out, sz); if (!t) break; out = t; }
        memcpy(out + pos, buf, len); pos += len;
    }
    out[pos] = 0; pclose(fp); return out;
}

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

SENSEI_STATUS don_behavior_analyze(uint32_t pid, SENSEI_DETECTION_LIST *results) {
    if (!results || pid == 0) return SENSEI_STATUS_ERROR;

    char cmd[256];
    char desc[SENSEI_MAX_DESCRIPTION];

    /* 1. Wakelock held too long */
    snprintf(cmd, sizeof(cmd),
        "cat /proc/%u/wakelocks 2>/dev/null | awk '$3>300000' | head -1", pid);
    char *wl = rish_cmd(cmd);
    if (wl && strlen(wl) > 2) {
        snprintf(desc, sizeof(desc),
            "PID %u holding wakelock >5min: %s", pid, wl);
        add_det(results, SENSEI_DETECTION_CLASS_BEHAVIOR, SENSEI_EVENT_PRIORITY_MEDIUM,
                SENSEI_MITRE_T1071, 75, "EXCESSIVE_WAKELOCK", desc);
    }
    free(wl);

    /* 2. Unexpected open sockets */
    snprintf(cmd, sizeof(cmd),
        "ls /proc/%u/fd 2>/dev/null | wc -l", pid);
    char *fds = rish_cmd(cmd);
    if (fds && atoi(fds) > 500) {
        snprintf(desc, sizeof(desc),
            "PID %u has %s open file descriptors — possible fd leak or exfil", pid, fds);
        add_det(results, SENSEI_DETECTION_CLASS_BEHAVIOR, SENSEI_EVENT_PRIORITY_MEDIUM,
                SENSEI_MITRE_T1071, 70, "EXCESSIVE_FDS", desc);
    }
    free(fds);

    /* 3. Process reading contacts/SMS/location without UI */
    snprintf(cmd, sizeof(cmd),
        "cat /proc/%u/status 2>/dev/null | grep -E '^State:' | grep -v 'S (sleeping)'", pid);
    char *state = rish_cmd(cmd);
    if (state && strstr(state, "R (running)")) {
        /* Check if it has sensitive permissions */
        snprintf(cmd, sizeof(cmd),
            "cat /proc/%u/cmdline 2>/dev/null | tr '\\0' ' ' | head -c 64", pid);
        char *cmdline = rish_cmd(cmd);
        if (cmdline && strlen(cmdline) > 2) {
            /* Check appops for this package */
            char pkg[128] = {0};
            strncpy(pkg, cmdline, sizeof(pkg)-1);
            pkg[strcspn(pkg, " \n")] = 0;
            char appops_cmd[256];
            snprintf(appops_cmd, sizeof(appops_cmd),
                "appops get %s READ_CONTACTS 2>/dev/null | grep -c 'allow'", pkg);
            char *contacts = rish_cmd(appops_cmd);
            if (contacts && atoi(contacts) > 0) {
                snprintf(desc, sizeof(desc),
                    "PID %u (%s) actively running with READ_CONTACTS permission", pid, pkg);
                add_det(results, SENSEI_DETECTION_CLASS_BEHAVIOR, SENSEI_EVENT_PRIORITY_MEDIUM,
                        SENSEI_MITRE_T1071, 65, "ACTIVE_CONTACTS_READ", desc);
            }
            free(contacts);
        }
        free(cmdline);
    }
    free(state);

    /* 4. Process with RECORD_AUDIO running in background */
    snprintf(cmd, sizeof(cmd),
        "cat /proc/%u/cmdline 2>/dev/null | tr '\\0' ' ' | head -c 64", pid);
    char *cmdline = rish_cmd(cmd);
    if (cmdline && strlen(cmdline) > 2) {
        char pkg[128] = {0};
        strncpy(pkg, cmdline, sizeof(pkg)-1);
        pkg[strcspn(pkg, " \n")] = 0;
        /* Skip system packages */
        if (!strstr(pkg, "com.android") && !strstr(pkg, "com.google") &&
            !strstr(pkg, "system") && strlen(pkg) > 3) {
            char audio_cmd[256];
            snprintf(audio_cmd, sizeof(audio_cmd),
                "appops get %s RECORD_AUDIO 2>/dev/null | grep -c 'allow'", pkg);
            char *audio = rish_cmd(audio_cmd);
            if (audio && atoi(audio) > 0) {
                snprintf(desc, sizeof(desc),
                    "PID %u (%s) has RECORD_AUDIO — check if mic use is expected", pid, pkg);
                add_det(results, SENSEI_DETECTION_CLASS_BEHAVIOR, SENSEI_EVENT_PRIORITY_HIGH,
                        SENSEI_MITRE_T1071, 75, "BACKGROUND_MIC_ACCESS", desc);
            }
            free(audio);
        }
    }
    free(cmdline);

    return SENSEI_STATUS_OK;
}
