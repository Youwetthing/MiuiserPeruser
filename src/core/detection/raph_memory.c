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


void raph_memory_scan(uint32_t pid, SENSEI_DETECTION_LIST *results) {
    april_log("INFO", "RAPH_MEM: Starting memory region scan...");

    /* Scan all PIDs if pid==0 */
    char *pids = rish_cmd("ls /proc 2>/dev/null | grep -E '^[0-9]+$' | head -50");
    if (!pids) return;

    char *tok = strtok(pids, "\n");
    while (tok) {
        uint32_t p = (uint32_t)atoi(tok);
        if (p > 0) {
            char cmd[256];
            snprintf(cmd, sizeof(cmd),
                "cat /proc/%u/maps 2>/dev/null | grep -E '^[0-9a-f]+-[0-9a-f]+ rwxp 00000000 00:00 0 *$' | head -1", p);
            char *out = rish_cmd(cmd);
            if (out && strlen(out) > 0) {
                char desc[SENSEI_MAX_DESCRIPTION];
                snprintf(desc, sizeof(desc),
                    "Anonymous RWX memory in PID %u — possible injector", p);
                add_det(results, SENSEI_DETECTION_CLASS_MEMORY, SENSEI_EVENT_PRIORITY_HIGH,
                        SENSEI_MITRE_T1055, 85, "ANON_RWX_MEM", desc);
                april_log("WARN", "RAPH_MEM: Anon RWX pages in PID %u", p);
            }
            free(out);

            snprintf(cmd, sizeof(cmd),
                "cat /proc/%u/maps 2>/dev/null | grep -iE 'frida-agent.so|linjector.so|xposed' | head -1", p);
            out = rish_cmd(cmd);
            if (out && strlen(out) > 0) {
                char desc[SENSEI_MAX_DESCRIPTION];
                snprintf(desc, sizeof(desc),
                    "Injector library in PID %u maps: %s", p, out);
                add_det(results, SENSEI_DETECTION_CLASS_HOOK, SENSEI_EVENT_PRIORITY_CRITICAL,
                        SENSEI_MITRE_T1055, 99, "INJECTOR_LIB", desc);
                april_log("THREAT", "RAPH_MEM: Injector in PID %u", p);
            }
            free(out);

            snprintf(cmd, sizeof(cmd),
                "cat /proc/%u/maps 2>/dev/null | grep ' /data/' | grep -v 'apk\\|dex\\|oat\\|art\\|jar' | grep 'rwxp\\|r-xp' | head -1", p);
            out = rish_cmd(cmd);
            if (out && strlen(out) > 0) {
                char desc[SENSEI_MAX_DESCRIPTION];
                snprintf(desc, sizeof(desc),
                    "Suspicious exec mapping from /data in PID %u", p);
                add_det(results, SENSEI_DETECTION_CLASS_MEMORY, SENSEI_EVENT_PRIORITY_HIGH,
                        SENSEI_MITRE_T1055, 80, "EXEC_FROM_DATA", desc);
                april_log("WARN", "RAPH_MEM: Exec from /data in PID %u", p);
            }
            free(out);
        }
        tok = strtok(NULL, "\n");
    }
    free(pids);
    april_log("INFO", "RAPH_MEM: Memory scan complete");
}
