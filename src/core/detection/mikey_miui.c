/*
 * mikey_miui.c — MIKEY: HyperOS/MIUI telemetry and tracking detection
 * Based on: TCD academic study (Leith 2021), FreeFromMi, XDA research
 * EEA/GDPR context: Redmi 15C, HyperOS OS2.0, Ireland
 */
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

static char *prop(const char *key) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "getprop %s 2>/dev/null", key);
    char *out = rish_cmd(cmd);
    if (out) out[strcspn(out, "\n")] = 0;
    return out;
}

SENSEI_STATUS mikey_miui(SENSEI_DETECTION_LIST *results) {
    if (!results) return SENSEI_STATUS_ERROR;
    int findings = 0;
    april_log("INFO", "MIKEY: Starting HyperOS/MIUI behavioural audit...");

    /* ── 1. SNO — Xiaomi device tracking ID, survives factory reset ── */
    char *sno = prop("ro.miui.cust.sno");
    if (!sno || strlen(sno) < 2) { free(sno); sno = prop("persist.sys.miui.sno"); }
    if (sno && strlen(sno) > 2) {
        char desc[SENSEI_MAX_DESCRIPTION];
        snprintf(desc, sizeof(desc),
            "Device SNO tracking ID present: %s — persists across factory reset, used for cross-device ad attribution", sno);
        add_det(results, SENSEI_DETECTION_CLASS_BEHAVIOR, SENSEI_EVENT_PRIORITY_LOW,
                SENSEI_MITRE_T1071, 90, "SNO_TRACKING", desc);
        april_log("INFO", "MIKEY: SNO present: %s", sno);
        findings++;
    }
    free(sno);

    /* ── 2. Facebook Partner ID — pre-authorises FB data collection ── */
    char *fb = prop("persist.sys.facebook.partnerid");
    if (!fb || strlen(fb) < 2) { free(fb); fb = prop("ro.facebook.partnerid"); }
    if (fb && strlen(fb) > 2) {
        char desc[SENSEI_MAX_DESCRIPTION];
        snprintf(desc, sizeof(desc),
            "Facebook Partner ID baked into device: %s — pre-authorises Facebook data collection at hardware level", fb);
        add_det(results, SENSEI_DETECTION_CLASS_BEHAVIOR, SENSEI_EVENT_PRIORITY_MEDIUM,
                SENSEI_MITRE_T1071, 95, "FB_PARTNER_ID", desc);
        april_log("WARN", "MIKEY: Facebook Partner ID: %s", fb);
        findings++;
    }
    free(fb);

    /* ── 3. Partner tokens — AppsFlyer, Netflix, Google, Spotify ── */
    static const struct { const char *prop; const char *name; } TOKENS[] = {
        { "ro.appsflyer.preinstall.path",   "AppsFlyer preinstall tracker"      },
        { "ro.netflix.channel",              "Netflix channel tracking ID"        },
        { "ro.com.google.clientidbase",      "Google client base (xiaomi-branded)" },
        { "ro.spotify.partner",              "Spotify partner token"              },
        { "ro.microsoft.preinstall_partner", "Microsoft preinstall partner"       },
        { NULL, NULL }
    };
    for (int i = 0; TOKENS[i].prop; i++) {
        char *val = prop(TOKENS[i].prop);
        if (val && strlen(val) > 2) {
            char desc[SENSEI_MAX_DESCRIPTION];
            snprintf(desc, sizeof(desc),
                "%s present: %s — pre-installed tracking token", TOKENS[i].name, val);
            add_det(results, SENSEI_DETECTION_CLASS_BEHAVIOR, SENSEI_EVENT_PRIORITY_LOW,
                    SENSEI_MITRE_T1071, 85, "PARTNER_TOKEN", desc);
            april_log("INFO", "MIKEY: Partner token — %s: %s", TOKENS[i].name, val);
            findings++;
        }
        free(val);
    }

    /* ── 4. MSA active — MIUI System Ads (TCD study: sends to api.ad.intl.xiaomi.com) ── */
    char *msa = rish_cmd("dumpsys activity processes 2>/dev/null | grep -c com.miui.msa.global");
    if (msa && atoi(msa) > 0) {
        add_det(results, SENSEI_DETECTION_CLASS_BEHAVIOR, SENSEI_EVENT_PRIORITY_HIGH,
                SENSEI_MITRE_T1071, 90, "MSA_ACTIVE",
                "MSA (MIUI System Ads) running — transmits encrypted data to api.ad.intl.xiaomi.com per TCD 2021 study");
        april_log("WARN", "MIKEY: MSA active — Xiaomi ad telemetry running");
        findings++;
    }
    free(msa);

    /* ── 5. MIUI Analytics — sends to data.mistat.intl.xiaomi.com ── */
    char *analytics = rish_cmd("dumpsys activity processes 2>/dev/null | grep -c com.miui.analytics");
    if (analytics && atoi(analytics) > 0) {
        add_det(results, SENSEI_DETECTION_CLASS_BEHAVIOR, SENSEI_EVENT_PRIORITY_MEDIUM,
                SENSEI_MITRE_T1071, 88, "MIUI_ANALYTICS",
                "MIUI Analytics running — screen touches and interaction logs sent to data.mistat.intl.xiaomi.com");
        april_log("WARN", "MIKEY: MIUI Analytics active");
        findings++;
    }
    free(analytics);

    /* ── 6. MiuiDaemon — background data collection ── */
    char *daemon = rish_cmd("dumpsys activity processes 2>/dev/null | grep -c com.miui.daemon");
    if (daemon && atoi(daemon) > 0) {
        add_det(results, SENSEI_DETECTION_CLASS_BEHAVIOR, SENSEI_EVENT_PRIORITY_MEDIUM,
                SENSEI_MITRE_T1071, 85, "MIUI_DAEMON",
                "MiuiDaemon running — background device data collection service");
        april_log("WARN", "MIKEY: MiuiDaemon active");
        findings++;
    }
    free(daemon);

    /* ── 7. GetApps / Xiaomi Discover — bypasses hosts file blocking ── */
    char *getapps = rish_cmd("dumpsys activity processes 2>/dev/null | grep -c com.xiaomi.discover");
    if (getapps && atoi(getapps) > 0) {
        add_det(results, SENSEI_DETECTION_CLASS_BEHAVIOR, SENSEI_EVENT_PRIORITY_MEDIUM,
                SENSEI_MITRE_T1071, 80, "XIAOMI_DISCOVER",
                "Xiaomi Discover/GetApps running — known to bypass DNS blocking for telemetry");
        april_log("WARN", "MIKEY: Xiaomi Discover active — bypasses hosts file");
        findings++;
    }
    free(getapps);

    /* ── 8. Guard Provider / Security Center ── */
    char *guard = rish_cmd("dumpsys activity processes 2>/dev/null | grep -c com.miui.guardprovider");
    if (guard && atoi(guard) > 0) {
        add_det(results, SENSEI_DETECTION_CLASS_BEHAVIOR, SENSEI_EVENT_PRIORITY_LOW,
                SENSEI_MITRE_T1071, 70, "GUARD_PROVIDER",
                "Guard Provider active — Xiaomi security scanning with cloud telemetry component");
        april_log("INFO", "MIKEY: Guard Provider active");
        findings++;
    }
    free(guard);

    /* ── 9. MILLET netlink scheduler kernel modules ── */
    char *millet = rish_cmd("lsmod 2>/dev/null | grep -c millet");
    if (millet && atoi(millet) > 0) {
        add_det(results, SENSEI_DETECTION_CLASS_KERNEL, SENSEI_EVENT_PRIORITY_MEDIUM,
                SENSEI_MITRE_T1014, 85, "MILLET_ACTIVE",
                "MILLET kernel scheduler active — Xiaomi process prioritisation with binder monitoring");
        april_log("WARN", "MIKEY: MILLET kernel modules loaded");
        findings++;
    }
    free(millet);

    /* ── 10. XiaoAI voice assistant active ── */
    char *xiaoai = rish_cmd("dumpsys activity processes 2>/dev/null | grep -c com.miui.voiceassist");
    if (xiaoai && atoi(xiaoai) > 0) {
        add_det(results, SENSEI_DETECTION_CLASS_BEHAVIOR, SENSEI_EVENT_PRIORITY_MEDIUM,
                SENSEI_MITRE_T1071, 80, "XIAOAI_ACTIVE",
                "XiaoAI voice assistant running — microphone access, cloud speech processing");
        april_log("WARN", "MIKEY: XiaoAI active — mic + cloud");
        findings++;
    }
    free(xiaoai);

    /* ── 11. Mi Ditto / Mi Share / MiLink — cross-device data bridge ── */
    char *milink = rish_cmd("dumpsys activity processes 2>/dev/null | grep -c com.milink.service");
    if (milink && atoi(milink) > 0) {
        add_det(results, SENSEI_DETECTION_CLASS_BEHAVIOR, SENSEI_EVENT_PRIORITY_LOW,
                SENSEI_MITRE_T1071, 70, "MILINK_ACTIVE",
                "MiLink service running — Xiaomi cross-device data bridge (clipboard sync, file transfer)");
        april_log("INFO", "MIKEY: MiLink/Mi Ditto active");
        findings++;
    }
    free(milink);

    /* ── 12. Mi Cloud sync active ── */
    char *cloud = rish_cmd("dumpsys activity processes 2>/dev/null | grep -c com.miui.micloudsync");
    if (cloud && atoi(cloud) > 0) {
        add_det(results, SENSEI_DETECTION_CLASS_BEHAVIOR, SENSEI_EVENT_PRIORITY_LOW,
                SENSEI_MITRE_T1071, 75, "MICLOUD_SYNC",
                "Mi Cloud sync running — contacts, SMS, photos synced to Xiaomi servers");
        april_log("INFO", "MIKEY: Mi Cloud sync active");
        findings++;
    }
    free(cloud);

    /* ── 13. EEA/GDPR opt-out status ── */
    char *gdpr = prop("persist.sys.miui.gdpr");
    if (!gdpr || strlen(gdpr) < 1 || strcmp(gdpr, "1") != 0) {
        add_det(results, SENSEI_DETECTION_CLASS_BEHAVIOR, SENSEI_EVENT_PRIORITY_MEDIUM,
                SENSEI_MITRE_T1071, 70, "GDPR_OPT_OUT",
                "MIUI GDPR opt-out not confirmed — telemetry may be active under EEA law");
        april_log("WARN", "MIKEY: GDPR opt-out status unclear");
        findings++;
    }
    free(gdpr);

    april_log("INFO", "MIKEY: HyperOS audit complete — %d findings", findings);
    return SENSEI_STATUS_OK;
}
