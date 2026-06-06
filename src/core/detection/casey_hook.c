#include "compat/sensei_compat.h"
#include "leo_detection.h"
#include "sensei_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

extern void april_log(const char* level, const char* format, ...);
extern int april_detection_list_append(SENSEI_DETECTION_LIST *list, const SENSEI_DETECTION *det);

static void check_frida(SENSEI_DETECTION_LIST *r);
static void check_xposed(SENSEI_DETECTION_LIST *r);
static void check_magisk(SENSEI_DETECTION_LIST *r);
static void check_ld_preload(SENSEI_DETECTION_LIST *r);
static void check_overlay_keylogger(SENSEI_DETECTION_LIST *r);

static void add_det(SENSEI_DETECTION_LIST *r,
                    SENSEI_DETECTION_CLASS cls,
                    SENSEI_EVENT_PRIORITY pri,
                    SENSEI_MITRE_TECHNIQUE mitre,
                    int conf, const char *type, const char *desc) {
    SENSEI_DETECTION det = {0};
    det.detection_class = cls;
    det.priority        = pri;
    det.mitre_id        = mitre;
    det.confidence      = conf;
    strncpy(det.detection_type, type, SENSEI_MAX_DETECTION_TYPE - 1);
    strncpy(det.description,    desc, SENSEI_MAX_DESCRIPTION    - 1);
    april_detection_list_append(r, &det);
}

/* Direct popen — bypasses shell_bridge entirely, no escaping corruption */
static char *direct_exec(const char *cmd) {
    char full[2048];
    snprintf(full, sizeof(full),
        "RISH_APPLICATION_ID=com.termux "
        "/data/data/com.termux/files/home/Rish/rish -c '%s' 2>/dev/null", cmd);
    FILE *fp = popen(full, "r");
    if (!fp) return NULL;
    size_t sz = 4096;
    char *out = malloc(sz);
    if (!out) { pclose(fp); return NULL; }
    out[0] = 0;
    size_t pos = 0;
    char buf[256];
    while (fgets(buf, sizeof(buf), fp)) {
        size_t len = strlen(buf);
        if (pos + len + 1 >= sz) {
            sz *= 2;
            char *tmp = realloc(out, sz);
            if (!tmp) break;
            out = tmp;
        }
        memcpy(out + pos, buf, len);
        pos += len;
    }
    out[pos] = 0;
    pclose(fp);
    return out;
}

/* Double-check: only fire if confirmed twice with 500ms gap */
static int cmd_hit(const char *cmd) {
    char *o1 = direct_exec(cmd);
    int hit1 = (o1 && strlen(o1) > 0);
    free(o1);
    if (!hit1) return 0;
    usleep(500000);
    char *o2 = direct_exec(cmd);
    int hit2 = (o2 && strlen(o2) > 0);
    free(o2);
    return hit2;
}

/* -- FRIDA -- */
static void check_frida(SENSEI_DETECTION_LIST *r) {
    if (cmd_hit("ps -A 2>/dev/null | grep -v grep | grep ' frida-server'")) {
        add_det(r, SENSEI_DETECTION_CLASS_HOOK, SENSEI_EVENT_PRIORITY_CRITICAL,
                SENSEI_MITRE_T1055, 99, "FRIDA_SERVER",
                "frida-server process found - dynamic instrumentation active");
        april_log("THREAT", "CASEY: Frida server detected");
    }
    if (cmd_hit("cat /proc/net/tcp6 /proc/net/tcp 2>/dev/null | grep ' 0A ' | awk '{print $2}' | grep -i ':699A'")) {
        add_det(r, SENSEI_DETECTION_CLASS_HOOK, SENSEI_EVENT_PRIORITY_CRITICAL,
                SENSEI_MITRE_T1055, 97, "FRIDA_PORT",
                "Port 27042 open - default Frida server port");
        april_log("THREAT", "CASEY: Frida port 27042 detected");
    }
    if (cmd_hit("cat /proc/*/maps 2>/dev/null | grep '/frida-agent.so'")) {
        add_det(r, SENSEI_DETECTION_CLASS_HOOK, SENSEI_EVENT_PRIORITY_CRITICAL,
                SENSEI_MITRE_T1055, 99, "FRIDA_AGENT",
                "frida-agent.so loaded in process memory");
        april_log("THREAT", "CASEY: frida-agent.so in maps");
    }
    if (cmd_hit("cat /proc/*/maps 2>/dev/null | grep -E '^[0-9a-f]+-[0-9a-f]+ rwxp 00000000 00:00 0 *$'")) {
        add_det(r, SENSEI_DETECTION_CLASS_HOOK, SENSEI_EVENT_PRIORITY_HIGH,
                SENSEI_MITRE_T1055, 85, "ANON_RWX_MEM",
                "Anonymous RWX pages found - possible injector artefact");
        april_log("WARN", "CASEY: Anon RWX memory detected");
    }
}

/* -- XPOSED -- */
static void check_xposed(SENSEI_DETECTION_LIST *r) {
    if (cmd_hit("pm list packages 2>/dev/null | grep '^package:.*lsposed\\|^package:.*xposed\\|^package:.*edxposed'")) {
        add_det(r, SENSEI_DETECTION_CLASS_HOOK, SENSEI_EVENT_PRIORITY_CRITICAL,
                SENSEI_MITRE_T1055, 99, "XPOSED_PKG",
                "Xposed/LSPosed package installed - Zygote hooking framework");
        april_log("THREAT", "CASEY: Xposed package detected");
    }
    if (cmd_hit("test -f /system/framework/XposedBridge.jar && echo yes")) {
        add_det(r, SENSEI_DETECTION_CLASS_HOOK, SENSEI_EVENT_PRIORITY_CRITICAL,
                SENSEI_MITRE_T1055, 99, "XPOSED_JAR",
                "XposedBridge.jar in /system/framework");
        april_log("THREAT", "CASEY: XposedBridge.jar found");
    }
}

/* -- MAGISK (informational only) -- */
static void check_magisk(SENSEI_DETECTION_LIST *r) {
    if (cmd_hit("test -d /data/adb/magisk && echo yes")) {
        add_det(r, SENSEI_DETECTION_CLASS_ROOTKIT, SENSEI_EVENT_PRIORITY_LOW,
                SENSEI_MITRE_T1014, 50, "MAGISK_FILES",
                "Magisk files found - expected on intentionally rooted devices");
        april_log("INFO", "CASEY: Magisk present (informational)");
    }
    if (cmd_hit("test -e /sys/kernel/ksu && echo yes")) {
        add_det(r, SENSEI_DETECTION_CLASS_ROOTKIT, SENSEI_EVENT_PRIORITY_LOW,
                SENSEI_MITRE_T1014, 50, "KERNELSU",
                "KernelSU detected - expected if intentionally installed");
        april_log("INFO", "CASEY: KernelSU present (informational)");
    }
}

/* -- LD_PRELOAD -- */
static void check_ld_preload(SENSEI_DETECTION_LIST *r) {
    if (cmd_hit("cat /proc/1/environ 2>/dev/null | tr '\\0' '\\n' | grep '^LD_PRELOAD='")) {
        add_det(r, SENSEI_DETECTION_CLASS_HOOK, SENSEI_EVENT_PRIORITY_CRITICAL,
                SENSEI_MITRE_T1055, 95, "LD_PRELOAD_INIT",
                "LD_PRELOAD set on init process - persistent library injection");
        april_log("THREAT", "CASEY: LD_PRELOAD on PID 1");
    }
}

/* -- OVERLAY / ACCESSIBILITY -- */
static void check_overlay_keylogger(SENSEI_DETECTION_LIST *r) {
    char *a11y = direct_exec("settings get secure enabled_accessibility_services 2>/dev/null");
    if (a11y && strlen(a11y) > 2 && strcmp(a11y, "null\n") != 0) {
        static const char *safe[] = {
            "com.google.", "com.android.", "com.miui.",
            "com.xiaomi.", "com.samsung.", NULL
        };
        int safe_pkg = 0;
        for (int i = 0; safe[i]; i++) {
            if (strstr(a11y, safe[i])) { safe_pkg = 1; break; }
        }
        if (!safe_pkg) {
            SENSEI_DETECTION det = {0};
            det.detection_class = SENSEI_DETECTION_CLASS_BEHAVIOR;
            det.priority        = SENSEI_EVENT_PRIORITY_HIGH;
            det.mitre_id        = SENSEI_MITRE_T1055;
            det.confidence      = 88;
            strncpy(det.detection_type, "SUSPICIOUS_A11Y", SENSEI_MAX_DETECTION_TYPE - 1);
            snprintf(det.description, SENSEI_MAX_DESCRIPTION - 1,
                     "Unknown accessibility service: %s", a11y);
            april_detection_list_append(r, &det);
            april_log("WARN", "CASEY: Suspicious a11y service");
        }
    }
    free(a11y);

    /* Notification listeners */
    char *nl = direct_exec("settings get secure enabled_notification_listeners 2>/dev/null");
    if (nl && strlen(nl) > 2 && strcmp(nl, "null\n") != 0) {
        static const char *safe_nl[] = {
            "com.google.", "com.android.", "com.miui.",
            "com.xiaomi.", NULL
        };
        int safe_pkg = 0;
        for (int i = 0; safe_nl[i]; i++) {
            if (strstr(nl, safe_nl[i])) { safe_pkg = 1; break; }
        }
        if (!safe_pkg) {
            SENSEI_DETECTION det = {0};
            det.detection_class = SENSEI_DETECTION_CLASS_BEHAVIOR;
            det.priority        = SENSEI_EVENT_PRIORITY_HIGH;
            det.mitre_id        = SENSEI_MITRE_T1055;
            det.confidence      = 85;
            strncpy(det.detection_type, "NOTIF_LISTENER", SENSEI_MAX_DETECTION_TYPE - 1);
            snprintf(det.description, SENSEI_MAX_DESCRIPTION - 1,
                     "Unknown notification listener: %s", nl);
            april_detection_list_append(r, &det);
            april_log("WARN", "CASEY: Unknown notification listener");
        }
    }
    free(nl);
}

/* -- Main entry -- */
SENSEI_STATUS casey_hook_check(SENSEI_DETECTION_LIST *results) {
    if (!results) return SENSEI_STATUS_ERROR;
    april_log("INFO", "CASEY: Starting hook & instrumentation scan...");
    check_frida(results);
    check_xposed(results);
    check_magisk(results);
    check_ld_preload(results);
    check_overlay_keylogger(results);
    april_log("INFO", "CASEY: Hook scan complete");
    return SENSEI_STATUS_OK;
}
