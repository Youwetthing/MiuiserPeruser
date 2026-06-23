/*
 * mitre_map.c — Signal → ATT&CK Mobile technique mapping
 * MiuiserPeruser | Gaveld judicial pipeline enrichment layer
 *
 * Technique data sourced from MITRE ATT&CK Mobile v16
 * https://attack.mitre.org/matrices/mobile/
 * Resources: mobile-attack.json (STIX 2.1)
 */

#include "mitre_map.h"
#include <string.h>

static const mitre_entry_t g_map[] = {
    /* ── Kernel / Memory integrity ──────────────────────────────── */
    { "KERNEL_MODULE_UNKNOWN",  "T1406",   "defense-evasion",      "Obfuscated Files or Information" },
    { "ROOTKIT_MODULE",         "T1406",   "defense-evasion",      "Obfuscated Files or Information" },
    { "ANON_RWX_MEM",           "T1631",   "defense-evasion",      "Process Injection" },
    { "INJECTOR_LIB",           "T1617",   "defense-evasion",      "Hooking" },
    { "EXEC_FROM_DATA",         "T1540",   "persistence",          "Code Injection" },
    { "MILLET_ACTIVE",          "T1631",   "defense-evasion",      "Process Injection" },

    /* ── Network / Connectivity ──────────────────────────────────── */
    { "SUSPECT_PORT",           "T1421",   "discovery",            "System Network Connections Discovery" },
    { "ADB_TCP_ACTIVE",         "T1421",   "discovery",            "System Network Connections Discovery" },
    { "DNS_ANOMALY",            "T1521",   "command-and-control",  "Encrypted Channel" },
    { "CONNECTIVITY_DRIFT",     "T1410",   "collection",           "Network Traffic Capture or Redirection" },
    { "UNKNOWN_LISTENER",       "T1423",   "discovery",            "Network Service Scanning" },
    { "PRIVATE_DNS_INACTIVE",   "T1521",   "command-and-control",  "Encrypted Channel" },

    /* ── Xiaomi / MIUI telemetry ─────────────────────────────────── */
    { "FB_PARTNER_ID",          "T1643",   "impact",               "Generate Traffic from Victim" },
    { "PARTNER_TOKEN",          "T1643",   "impact",               "Generate Traffic from Victim" },
    { "SNO_TRACKING",           "T1643",   "impact",               "Generate Traffic from Victim" },
    { "MIUI_ANALYTICS",         "T1643",   "impact",               "Generate Traffic from Victim" },
    { "MIUI_DAEMON",            "T1426",   "discovery",            "System Information Discovery" },
    { "GDPR_OPT_OUT",           "T1426",   "discovery",            "System Information Discovery" },
    { "GUARD_PROVIDER",         "T1418",   "discovery",            "Software Discovery" },

    /* ── Process / Behaviour ─────────────────────────────────────── */
    { "EXCESSIVE_WAKELOCK",     "T1603",   "persistence",          "Scheduled Task/Job" },
    { "EXCESSIVE_FDS",          "T1424",   "discovery",            "Process Discovery" },
    { "ZOMBIE_PROCESSES",       "T1424",   "discovery",            "Process Discovery" },
    { "HIDDEN_GAP",             "T1424",   "discovery",            "Process Discovery" },

    /* ── Hook / Instrumentation ──────────────────────────────────── */
    { "FRIDA_DETECTED",         "T1617",   "defense-evasion",      "Hooking" },
    { "XPOSED_DETECTED",        "T1617",   "defense-evasion",      "Hooking" },

    /* ── Sentinel — keep last ────────────────────────────────────── */
    { NULL, NULL, NULL, NULL }
};

const mitre_entry_t *mitre_lookup(const char *signal_type) {
    if (!signal_type) return NULL;
    for (int i = 0; g_map[i].signal_type != NULL; i++) {
        if (strcmp(g_map[i].signal_type, signal_type) == 0)
            return &g_map[i];
    }
    return NULL;
}
