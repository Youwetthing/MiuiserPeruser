#include "compat/sensei_compat.h"
#include "sensei_types.h"
#include "rish_pipe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>

extern void april_log(const char* level, const char* format, ...);
extern SENSEI_STATUS april_detection_list_append(SENSEI_DETECTION_LIST *list, const SENSEI_DETECTION *det);

static char *rish_cmd(const char *cmd) {
    char full[2048];
    snprintf(full, sizeof(full),
        "RISH_APPLICATION_ID=com.termux "
        "/data/data/com.termux/files/home/Rish/rish -c '%s' 2>/dev/null", cmd);
    FILE *fp = popen(full, "r");
    if (!fp) return NULL;
    size_t sz = 8192; char *out = malloc(sz);
    if (!out) { pclose(fp); return NULL; }
    out[0] = 0; size_t pos = 0; char buf[512];
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


SENSEI_STATUS don_memorypressure_check(SENSEI_DETECTION_LIST *results) {
    char *out = rish_cmd("cat /proc/meminfo 2>/dev/null");
    if (!out) return SENSEI_STATUS_ERROR;

    long total = 0, available = 0;
    char *line = strtok(out, "\n");
    while (line) {
        if (strncmp(line, "MemTotal:", 9) == 0)     total     = atol(line + 9);
        if (strncmp(line, "MemAvailable:", 13) == 0) available = atol(line + 13);
        line = strtok(NULL, "\n");
    }
    free(out);

    if (total > 0) {
        int used_pct = (int)(100 - (available * 100 / total));
        if (used_pct >= 90) {
            char desc[SENSEI_MAX_DESCRIPTION];
            snprintf(desc, sizeof(desc),
                "RAM critically low (%d%% used). MIUI may start killing processes.", used_pct);
            add_det(results, SENSEI_DETECTION_CLASS_BEHAVIOR, SENSEI_EVENT_PRIORITY_CRITICAL,
                    SENSEI_MITRE_NONE, 90, "RAM_CRITICAL", desc);
            april_log("THREAT", "Woah Nelly! RAM critically low (%d%% used). MIUI may start killing processes.", used_pct);
        } else if (used_pct >= 80) {
            char desc[SENSEI_MAX_DESCRIPTION];
            snprintf(desc, sizeof(desc), "RAM pressure high (%d%% used)", used_pct);
            add_det(results, SENSEI_DETECTION_CLASS_BEHAVIOR, SENSEI_EVENT_PRIORITY_HIGH,
                    SENSEI_MITRE_NONE, 80, "RAM_HIGH", desc);
            april_log("WARN", "DON_MEM: RAM high (%d%%)", used_pct);
        }
    }
    return SENSEI_STATUS_OK;
}
