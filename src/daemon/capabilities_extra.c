/*
 * capabilities_extra.c — daemon capability detection
 *
 * Owns the full detect_capabilities() for daemon use.
 * Core platform/capabilities.c is NOT linked by the daemon — no collision.
 *
 * ADB and Shizuku probe logic ported from core/platform/capabilities.c.
 * Port bridge probed directly against MP_TCP_PORT — no splinter_protocol.h
 * dependency until that header exists.
 * rish and shizuku_script stubbed — wire when rish_pipe is ready.
 */

#include "capabilities_extra.h"
#include "port_pathway.h"
#include "ipc_globals.h"
#include "../core/log_safe.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

/* ── Global state ─────────────────────────────────────────────────────── */

capabilities_t capabilities;

/* ── Probe helpers ────────────────────────────────────────────────────── */

static bool cmd_has_output(const char *cmd)
{
    FILE *fp = popen(cmd, "r");
    if (!fp) return false;
    char buf[256];
    bool ok = (fgets(buf, sizeof(buf), fp) != NULL);
    pclose(fp);
    return ok;
}

/* ── ADB (ported from core, unchanged) ───────────────────────────────── */

static bool check_adb(void)
{
    FILE *fp = popen("adb get-state 2>/dev/null", "r");
    if (fp) {
        char buf[64] = {0};
        if (fgets(buf, sizeof(buf), fp)) {
            pclose(fp);
            if (strstr(buf, "device") || strstr(buf, "unauthorized"))
                return true;
        } else {
            pclose(fp);
        }
    }

    fp = popen("adb devices 2>/dev/null", "r");
    if (!fp) return false;

    char line[256];
    bool ok = false;
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "List of devices")) continue;
        if (strstr(line, "device") || strstr(line, "unauthorized")) {
            ok = true;
            break;
        }
    }
    pclose(fp);
    return ok;
}

/* ── Shizuku (ported from core, unchanged) ────────────────────────────── */

static bool check_shizuku(void)
{
    if (cmd_has_output("pm list packages | grep -i shizuku"))             return true;
    if (cmd_has_output("cmd -l 2>/dev/null | grep -i shizuku"))           return true;
    if (cmd_has_output("dumpsys activity services 2>/dev/null | grep -i moe.shizuku.server")) return true;
    if (cmd_has_output("ps -A 2>/dev/null | grep -E 'shizuku|app_process'")) return true;
    return false;
}

/* ── Port bridge (direct TCP probe against MP_TCP_PORT) ──────────────── */

static bool probe_port_bridge(void)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return false;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(MP_TCP_PORT);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    bool ok = (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    close(fd);
    return ok;
}

static bool probe_port_bridge_info(void)
{
    /* TODO: send INFO command and validate response once
     * splinter_protocol.h defines the exchange format */
    return false;
}

/* ── Public API ───────────────────────────────────────────────────────── */

void detect_capabilities(void)
{
    memset(&capabilities, 0, sizeof(capabilities));

    capabilities.adb     = check_adb()     ? 1 : 0;
    capabilities.shizuku = check_shizuku() ? 1 : 0;

    /* rish: stub until rish_pipe API is wired */
    capabilities.rish           = 0;
    /* shizuku_script: stub until shizuku scripting interface confirmed */
    capabilities.shizuku_script = 0;

    if (probe_port_bridge()) {
        capabilities.port_bridge_available = 1;
        if (probe_port_bridge_info()) {
            capabilities.port_bridge_info_ok = 1;
        }
    }

    log_safe("INFO",
        "Capabilities: adb=%d shizuku=%d rish=%d port_bridge=%d",
        capabilities.adb,
        capabilities.shizuku,
        capabilities.rish,
        capabilities.port_bridge_available);

    if (!capabilities.adb) {
        fprintf(stderr,
            "[Hint] ADB not detected.\n"
            "       Enable Developer Options → USB debugging → adb devices\n"
            "       https://developer.android.com/studio/command-line/adb\n\n");
    }

    if (!capabilities.shizuku) {
        fprintf(stderr,
            "[Hint] Shizuku not detected.\n"
            "       Start via root, ADB, or Wireless debugging.\n"
            "       https://github.com/RikkaApps/Shizuku\n\n");
    }
}

void capability_print_summary(void)
{
    printf("Summary:\n");
    printf("  ADB:                 %d\n", capabilities.adb);
    printf("  Shizuku:             %d\n", capabilities.shizuku);
    printf("  Shizuku script:      %d\n", capabilities.shizuku_script);
    printf("  Rish:                %d\n", capabilities.rish);
    printf("  Port bridge avail:   %d\n", capabilities.port_bridge_available);
    printf("  Port bridge info:    %d\n", capabilities.port_bridge_info_ok);
}

void capability_print_hints(void)
{
    if (!capabilities.port_bridge_available) {
        printf("Hint: No port bridge detected on 127.0.0.1:%d\n", MP_TCP_PORT);
    } else if (!capabilities.port_bridge_info_ok) {
        printf("Hint: Bridge responded but did not return info.\n");
    } else {
        printf("Hint: Port bridge fully operational.\n");
    }
}
