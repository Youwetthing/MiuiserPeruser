#include "compat/sensei_compat.h"
#include "sensei_types.h"
#include "rish_pipe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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
    size_t sz = 16384; char *out = malloc(sz);
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

/* Known suspicious ports */
static const struct { int port; const char *name; } SUSPECT_PORTS[] = {
    { 27042, "Frida server default port"           },
    { 27043, "Frida alt port"                      },
    {  4444, "Common RAT/backdoor port"             },
    {  1234, "Common debug/backdoor port"           },
    {  9999, "Common backdoor port"                 },
    {  5554, "Android emulator port"                },
    {  5555, "ADB TCP (should only be loopback)"    },
    {  7777, "Common trojan port"                   },
    { 31337, "Classic backdoor port (eleet)"        },
    { 0, NULL }
};

/* Xiaomi telemetry IP ranges (known) */
static const char *XIAOMI_RANGES[] = {
    "36.152.",   /* Xiaomi CN */
    "58.83.",    /* Xiaomi CN */
    "120.92.",   /* Xiaomi CN */
    "111.206.",  /* Xiaomi CN */
    "106.38.",   /* Xiaomi CN */
    NULL
};

static void uid_to_package(int uid, char *pkg, size_t sz) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd),
        "pm list packages -U 2>/dev/null | grep uid:%d | cut -d: -f2 | head -1", uid);
    char *out = rish_cmd(cmd);
    if (out && strlen(out) > 2) {
        strncpy(pkg, out, sz-1);
        pkg[strcspn(pkg, "\n")] = 0;
    } else {
        snprintf(pkg, sz, "uid=%d", uid);
    }
    free(out);
}

static int is_loopback(const char *addr) {
    return strstr(addr, "0100007F") != NULL ||
           strstr(addr, "00000000000000000000000001000000") != NULL;
}

SENSEI_STATUS raph_network_scan(SENSEI_DETECTION_LIST *results) {
    if (!results) return SENSEI_STATUS_ERROR;
    april_log("INFO", "RAPH: Starting network audit...");
    int ext_count = 0;

    /* Parse /proc/net/tcp and /proc/net/tcp6 */
    const char *files[] = { "tcp", "tcp6", NULL };
    for (int f = 0; files[f]; f++) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "cat /proc/net/%s 2>/dev/null", files[f]);
        char *raw = rish_cmd(cmd);
        if (!raw) continue;

        char *line = strtok(raw, "\n");
        int first = 1;
        while (line) {
            if (first) { first = 0; line = strtok(NULL, "\n"); continue; } /* skip header */

            char local[64]={0}, rem[64]={0}, state[8]={0};
            int uid = 0;
            sscanf(line, " %*d: %63s %63s %7s %*s %*s %*s %*s %*s %d",
                   local, rem, state, &uid);

            /* Only ESTABLISHED (01) and LISTEN (0A) */
            if (strcmp(state, "01") != 0 && strcmp(state, "0A") != 0) {
                line = strtok(NULL, "\n");
                continue;
            }

            /* Extract port from local address (after colon) */
            char *lport_s = strrchr(local, ':');
            int lport = lport_s ? (int)strtol(lport_s+1, NULL, 16) : 0;
            char *rport_s = strrchr(rem, ':');
            int rport = rport_s ? (int)strtol(rport_s+1, NULL, 16) : 0;

            /* Check suspicious ports */
            for (int i = 0; SUSPECT_PORTS[i].name; i++) {
                if (lport == SUSPECT_PORTS[i].port || rport == SUSPECT_PORTS[i].port) {
                    /* Allow ADB on loopback */
                    if (SUSPECT_PORTS[i].port == 5555 &&
                        (is_loopback(local) || is_loopback(rem))) {
                        break;
                    }
                    char pkg[128] = {0};
                    uid_to_package(uid, pkg, sizeof(pkg));
                    char desc[SENSEI_MAX_DESCRIPTION];
                    snprintf(desc, sizeof(desc),
                        "Suspicious port %d (%s) — uid=%d pkg=%s",
                        SUSPECT_PORTS[i].port, SUSPECT_PORTS[i].name, uid, pkg);
                    add_det(results, SENSEI_DETECTION_CLASS_NETWORK,
                            SENSEI_EVENT_PRIORITY_HIGH, SENSEI_MITRE_T1071, 85,
                            "SUSPECT_PORT", desc);
                    april_log("WARN", "RAPH: Suspicious port %d — %s (pkg=%s)",
                              SUSPECT_PORTS[i].port, SUSPECT_PORTS[i].name, pkg);
                    ext_count++;
                    break;
                }
            }
            line = strtok(NULL, "\n");
        }
        free(raw);
    }

    /* Check for active connections to Xiaomi telemetry domains */
    char *netstat = rish_cmd(
        "ss -tnp 2>/dev/null | grep ESTAB | awk '{print $5}' | grep -v '127\\.' | head -20");
    if (netstat && strlen(netstat) > 2) {
        char *tok = strtok(netstat, "\n");
        while (tok) {
            for (int i = 0; XIAOMI_RANGES[i]; i++) {
                if (strstr(tok, XIAOMI_RANGES[i])) {
                    char desc[SENSEI_MAX_DESCRIPTION];
                    snprintf(desc, sizeof(desc),
                        "Active connection to Xiaomi telemetry IP: %s", tok);
                    add_det(results, SENSEI_DETECTION_CLASS_NETWORK,
                            SENSEI_EVENT_PRIORITY_MEDIUM, SENSEI_MITRE_T1071, 80,
                            "XIAOMI_TELEMETRY_CONN", desc);
                    april_log("WARN", "RAPH: Xiaomi telemetry connection: %s", tok);
                    ext_count++;
                }
            }
            tok = strtok(NULL, "\n");
        }
    }
    free(netstat);

    /* Check for packet socket sniffers */
    /* Packet sockets — only flag if owned by non-system UIDs (>10000) */
    char *pkt = rish_cmd("cat /proc/net/packet 2>/dev/null | awk 'NR>1 && $7>10000 {print $7}' | sort -u");
    if (pkt && strlen(pkt) > 1) {
        char desc[SENSEI_MAX_DESCRIPTION];
        snprintf(desc, sizeof(desc),
            "Raw packet socket owned by app UID: %s — possible sniffer", pkt);
        add_det(results, SENSEI_DETECTION_CLASS_NETWORK,
                SENSEI_EVENT_PRIORITY_HIGH, SENSEI_MITRE_T1071, 85,
                "PACKET_SNIFFER", desc);
        april_log("WARN", "RAPH: Non-system raw packet socket: uid %s", pkt);
        ext_count++;
    }
    free(pkt);

    /* ADB over network check */
    char *adb = rish_cmd("getprop service.adb.tcp.port 2>/dev/null");
    if (adb && strlen(adb) > 0 && strcmp(adb, "\n") != 0 && strcmp(adb, "-1\n") != 0) {
        adb[strcspn(adb, "\n")] = 0;
        char desc[SENSEI_MAX_DESCRIPTION];
        snprintf(desc, sizeof(desc), "ADB TCP port active: %s — remote shell access possible", adb);
        add_det(results, SENSEI_DETECTION_CLASS_NETWORK,
                SENSEI_EVENT_PRIORITY_MEDIUM, SENSEI_MITRE_T1071, 70,
                "ADB_TCP_ACTIVE", desc);
        april_log("INFO", "RAPH: ADB TCP port: %s (informational)", adb);
    }
    free(adb);

    april_log("INFO", "RAPH: %d external connections active", ext_count);
    april_log("INFO", "RAPH: Network audit complete");
    return SENSEI_STATUS_OK;
}
