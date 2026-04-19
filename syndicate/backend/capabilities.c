#include "capabilities.h"
#include "rish_pipe.h"
#include "fugitoid_log.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* --- Global state --- */
capability_state_t capabilities;

/* --- Helper to run a command and check if any output exists --- */
static bool cmd_has_output(const char *cmd) {
    FILE *fp = popen(cmd, "r");
    if (!fp) return false;
    char buf[256];
    bool ok = false;
    if (fgets(buf, sizeof(buf), fp)) ok = true;
    pclose(fp);
    return ok;
}

/* --- ADB detection (simple, reliable) --- */
static bool check_adb(void) {
    /* Fast path: try get-state first (clean output when server running) */
    FILE *fp = popen("adb get-state 2>/dev/null", "r");
    if (fp) {
        char buf[64] = {0};
        if (fgets(buf, sizeof(buf), fp)) {
            pclose(fp);
            if (strstr(buf, "device") || strstr(buf, "unauthorized")) return true;
        } else {
            pclose(fp);
        }
    }

    /* Fallback: adb devices parsing (works for USB and wireless) */
    fp = popen("adb devices 2>/dev/null", "r");
    if (!fp) return false;

    char line[256];
    bool ok = false;
    while (fgets(line, sizeof(line), fp)) {
        /* skip header line */
        if (strstr(line, "List of devices")) continue;
        if (strstr(line, "device") || strstr(line, "unauthorized")) {
            ok = true;
            break;
        }
    }
    pclose(fp);
    return ok;
}

/* --- Shizuku detection (hybrid probes) --- */
static bool check_shizuku(void) {
    char cmd[512];

    /* 1) Package installed? (fast) */
    if (cmd_has_output("pm list packages | grep -i shizuku")) return true;

    /* 2) Binder/service list (fast, checks registered binder services) */
    if (cmd_has_output("cmd -l 2>/dev/null | grep -i shizuku")) return true;

    /* 3) dumpsys/dumpsys activity services for the Shizuku service (deeper) */
    if (cmd_has_output("dumpsys activity services 2>/dev/null | grep -i moe.shizuku.server")) return true;

    /* 4) Process-level check (root-started or app_process) */
    if (cmd_has_output("ps -A 2>/dev/null | grep -E 'shizuku|app_process'")) return true;

    /* Not found by any probe */
    return false;
}

/* --- Basic access checks (placeholders kept intentionally simple) --- */
static bool checkproc_access(void) { return true; }
static bool checksys_access(void)  { return true; }
static bool checkmiui_services(void) { return true; }

/* --- Public API --- */
void detect_capabilities(void) {
    capabilities.adb           = check_adb();
    capabilities.shizuku       = check_shizuku();
    capabilities.proc_access   = checkproc_access();
    capabilities.sys_access    = checksys_access();
    capabilities.miui_services = checkmiui_services();

    fugitoid_log("INFO",
        "Capabilities: adb=%d, shizuku=%d, proc=%d, sys=%d, miui=%d",
        capabilities.adb,
        capabilities.shizuku,
        capabilities.proc_access,
        capabilities.sys_access,
        capabilities.miui_services
    );

    /* Helpful guidance printed to stderr for interactive users */
    if (!capabilities.adb) {
        fprintf(stderr,
            "[Hint] ADB not detected.\n"
            "       To enable ADB:\n"
            "         1) Enable Developer Options on the device\n"
            "         2) Enable USB debugging\n"
            "         3) Run: adb devices\n"
            "         4) Approve the authorization prompt on the phone\n"
            "       More: https://developer.android.com/studio/command-line/adb\n\n"
        );
    }

    if (!capabilities.shizuku) {
        fprintf(stderr,
            "[Hint] Shizuku not detected.\n"
            "       To enable Shizuku (common methods):\n"
            "         • Start via root (if device rooted)\n"
            "         • Start via ADB: follow Shizuku's ADB start flow\n"
            "         • Start via Wireless debugging: pair and start service\n"
            "       Shizuku GitHub: https://github.com/RikkaApps/Shizuku\n\n"
        );
    }
}
