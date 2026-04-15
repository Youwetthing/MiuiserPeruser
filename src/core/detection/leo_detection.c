/*
 * MiuiserPeruser – Core detection engine (Leonardo)
 * With advanced battery health check via dumpsys batterystats
 */

#include <leo_detection.h>
#include <april_platform.h>
#include <april_event.h>
#include <sensei_types.h>
#include "rish_pipe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

#define CONFIG_FILE "/data/data/com.termux/files/home/.miuiserperuser_config"

/* External module declarations (the other turtles) */
extern SENSEI_STATUS raph_memory_scan(uint32_t pid, SENSEI_DETECTION_LIST *results);
extern SENSEI_STATUS raph_network_scan(SENSEI_DETECTION_LIST *results);
extern SENSEI_STATUS don_integrity_check(SENSEI_DETECTION_LIST *results);
extern SENSEI_STATUS don_behavior_analyze(uint32_t pid, SENSEI_DETECTION_LIST *results);
extern SENSEI_STATUS don_memorypressure_check(SENSEI_DETECTION_LIST *results);
extern SENSEI_STATUS casey_hook_check(SENSEI_DETECTION_LIST *results);
extern SENSEI_STATUS casey_kernel_check(SENSEI_DETECTION_LIST *results);
extern SENSEI_STATUS mikey_miui(SENSEI_DETECTION_LIST *results);

/* Logging (April's job) */
extern void april_log(const char* level, const char* format, ...);

/* Internal state */
static SENSEI_DETECTION_CONFIG g_config;
static bool g_engine_initialized = false;
static SENSEI_EVENT_QUEUE g_event_queue;

/* Whitelist */
static const char *g_whitelist[] = {
    "kworker", "rcu", "ksoftirqd", "migration", "watchdog", "kthreadd",
    "irq/", "spi", "mmcqd", "cmdq", "cpuhp", "idle_inject", "kauditd",
    "khungtaskd", "oom_reaper", "kcompactd", "kswapd", "erofs_worker",
    "dmabuf", "hang_detect", "wdtk", "teei_", "dma_pool",
    "kworker", "rcu", "ksoftirqd", "migration", "watchdog", "kthreadd",
    "irq/", "spi", "mmcqd", "cmdq", "cpuhp", "idle_inject", "kauditd",
    "khungtaskd", "oom_reaper", "kcompactd", "kswapd", "erofs_worker",
    "dmabuf", "hang_detect", "wdtk", "teei_", "dma_pool",
    "chrome.exe", "msedge.exe", "firefox.exe", "Code.exe",
    "discord.exe", "slack.exe", "Teams.exe", "node.exe",
    "electron.exe", "star-daemon.exe", "svchost.exe", "csrss.exe",
    "explorer.exe",
    "com.miui.securitycenter", "com.miui.securityadd", "com.miui.cleanmaster",
    NULL
};

static bool is_whitelisted(const char *name) {
    if (!name) return false;
    const char *fname = strrchr(name, '\\');
    if (fname) fname++; else fname = name;
    const char *slash = strrchr(fname, '/');
    if (slash) fname = slash + 1;
    for (int i = 0; g_whitelist[i]; i++) {
        if (strcasecmp(fname, g_whitelist[i]) == 0)
            return true;
    }
    return false;
}

/* Load configuration from file */
static void load_config(void) {
    FILE *f = fopen(CONFIG_FILE, "r");
    if (!f) {
        /* Defaults: all on except kernel */
        g_config.enable_memory_scan = 1;
        g_config.enable_hook_detection = 1;
        g_config.enable_behavior_analysis = 1;
        g_config.enable_kernel_analysis = 0;
        g_config.enable_network_monitor = 1;
        g_config.enable_integrity_monitor = 1;
        g_config.scan_interval_ms = 5000;
        return;
    }
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        char key[64];
        int val;
        if (sscanf(line, "%63[^=]=%d", key, &val) == 2) {
            if (strcmp(key, "memory") == 0) g_config.enable_memory_scan = val;
            else if (strcmp(key, "hook") == 0) g_config.enable_hook_detection = val;
            else if (strcmp(key, "behavior") == 0) g_config.enable_behavior_analysis = val;
            else if (strcmp(key, "kernel") == 0) g_config.enable_kernel_analysis = val;
            else if (strcmp(key, "network") == 0) g_config.enable_network_monitor = val;
            else if (strcmp(key, "integrity") == 0) g_config.enable_integrity_monitor = val;
        }
    }
    fclose(f);
    april_log("INFO", "Configuration loaded.");
}

/* Reload config (called from IPC) */
void leo_config_reload(void) {
    load_config();
}

SENSEI_STATUS leo_init(const SENSEI_DETECTION_CONFIG *config) {
    if (g_engine_initialized) return SENSEI_STATUS_OK;
    if (config) {
        memcpy(&g_config, config, sizeof(g_config));
    } else {
        load_config();
    }
    if (april_event_queue_init(&g_event_queue) != SENSEI_STATUS_OK)
        return SENSEI_STATUS_ERROR;
    g_engine_initialized = true;
    april_log("INFO", "MiuiserPeruser engine initialized.");
    return SENSEI_STATUS_OK;
}

void leo_shutdown(void) {
    if (!g_engine_initialized) return;
    april_event_queue_destroy(&g_event_queue);
    g_engine_initialized = false;
    april_log("INFO", "Engine shut down.");
}

/* Internal per‑process scan */
static SENSEI_STATUS scan_one_process(uint32_t pid, SENSEI_DETECTION_LIST *results) {
    if (!g_engine_initialized) return SENSEI_STATUS_ERROR;
    if (g_config.enable_memory_scan)
        raph_memory_scan(pid, results);
    if (g_config.enable_behavior_analysis)
        don_behavior_analyze(pid, results);
    return SENSEI_STATUS_OK;
}

/* ============================================================
 * Enhanced checks using rish
 * ============================================================ */

static void check_selinux(SENSEI_DETECTION_LIST *results) {
    char *output = rish_pipe_command("getenforce");
    if (!output) return;
    char *nl = strchr(output, '\n');
    if (nl) *nl = '\0';
    if (strcmp(output, "Enforcing") != 0) {
        SENSEI_DETECTION det = {0};
        det.detection_class = SENSEI_DETECTION_CLASS_BEHAVIOR;
        det.priority = SENSEI_EVENT_PRIORITY_MEDIUM;
        det.confidence = 90;
        det.mitre_id = SENSEI_MITRE_NONE;
        strncpy(det.detection_type, "SELINUX_PERMISSIVE", SENSEI_MAX_DETECTION_TYPE-1);
        snprintf(det.description, SENSEI_MAX_DESCRIPTION-1,
                 "SELinux is in %s mode. Should be Enforcing.", output);
        leo_detection_list_append(results, &det);
    }
    free(output);
}

static void check_cpu_frequencies(SENSEI_DETECTION_LIST *results) {
    char *output = rish_pipe_command("cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq 2>/dev/null");
    if (!output) return;
    int freqs[32];
    int count = 0;
    char *line = strtok(output, "\n");
    while (line && count < 32) {
        while (*line && !isdigit(*line)) line++;
        if (*line) {
            freqs[count++] = atoi(line);
        }
        line = strtok(NULL, "\n");
    }
    if (count == 0) {
        free(output);
        return;
    }
    int max_freq = 0;
    for (int i = 0; i < count; i++) {
        if (freqs[i] > max_freq) max_freq = freqs[i];
    }
    int threshold = max_freq * 80 / 100; // 80% of max
    int throttled = 0;
    for (int i = 0; i < count; i++) {
        if (freqs[i] < threshold) {
            throttled = 1;
            break;
        }
    }
    if (throttled) {
        SENSEI_DETECTION det = {0};
        det.detection_class = SENSEI_DETECTION_CLASS_BEHAVIOR;
        det.priority = SENSEI_EVENT_PRIORITY_MEDIUM;
        det.confidence = 70;
        det.mitre_id = SENSEI_MITRE_NONE;
        strncpy(det.detection_type, "CPU_THROTTLING", SENSEI_MAX_DETECTION_TYPE-1);
        snprintf(det.description, SENSEI_MAX_DESCRIPTION-1,
                 "Some CPU cores are throttled (max %d kHz, some at lower frequencies).", max_freq);
        leo_detection_list_append(results, &det);
    }
    free(output);
}

/* Advanced battery health using dumpsys batterystats est_capacity */
static void check_battery_health_advanced(SENSEI_DETECTION_LIST *results) {
    char *output = rish_pipe_command("dumpsys batterystats | grep -i est_capacity | tail -1");
    if (!output) return;
    char *eq = strstr(output, "est_capacity=");
    if (!eq) {
        free(output);
        return;
    }
    eq += 13; // length of "est_capacity="
    long est_cap = 0;
    while (*eq && isdigit(*eq)) {
        est_cap = est_cap * 10 + (*eq - '0');
        eq++;
    }
    free(output);
    if (est_cap == 0) return;
    int est_mah = est_cap / 10; // convert from tenths of mAh
    int design = 5160; // Redmi 15C design capacity
    int health = (est_mah * 100) / design;
    if (health < 80) {
        SENSEI_DETECTION det = {0};
        det.detection_class = SENSEI_DETECTION_CLASS_BEHAVIOR;
        det.priority = SENSEI_EVENT_PRIORITY_MEDIUM;
        det.confidence = 70;
        det.mitre_id = SENSEI_MITRE_NONE;
        strncpy(det.detection_type, "BATTERY_HEALTH_LOW", SENSEI_MAX_DETECTION_TYPE-1);
        snprintf(det.description, SENSEI_MAX_DESCRIPTION-1,
                 "Battery health estimated at %d%%. Consider replacement if below 80%%.", health);
        leo_detection_list_append(results, &det);
    }
    // Always log health for information (optional)
    char logmsg[128];
    snprintf(logmsg, sizeof(logmsg), "Battery health estimated at %d%%", health);
    april_log("INFO", logmsg);
}

/* ============================================================
 * Full scan
 * ============================================================ */
SENSEI_STATUS leo_full_scan(SENSEI_DETECTION_LIST *results) {
    if (!g_engine_initialized || !results) {
        april_log("CRIT", "Full scan called before init");
        return SENSEI_STATUS_ERROR;
    }

    april_log("INFO", "Starting full system scan...");

    /* Global checks */
    if (g_config.enable_network_monitor)
        raph_network_scan(results);
    if (g_config.enable_integrity_monitor)
        don_integrity_check(results);
    don_memorypressure_check(results);
    if (g_config.enable_hook_detection)
        casey_hook_check(results);
    if (g_config.enable_kernel_analysis)
        casey_kernel_check(results);

    /* MIUI‑specific (optional) */
    mikey_miui(results);   // disabled by default

    /* Enhanced rish‑based checks */
    check_selinux(results);
    check_cpu_frequencies(results);
    check_battery_health_advanced(results);

    /* Process enumeration */
    SENSEI_PROCESS_LIST processes = {0};
    if (april_enum_processes(&processes) == SENSEI_STATUS_OK) {
        SENSEI_PROCESS_INFO *cur = processes.head;
        int scanned = 0;
        while (cur) {
            if (!is_whitelisted(cur->name)) {
                april_log("INFO", "Scanning %s (PID %u)", cur->name, cur->pid);
                scan_one_process(cur->pid, results);
                scanned++;
            }
            cur = cur->next;
        }
        april_log("INFO", "Process scan complete: %d processes examined.", scanned);
        april_process_list_free(&processes);
    } else {
        april_log("WARN", "Failed to enumerate processes (permissions?).");
    }

    april_log("INFO", "Full scan finished.");
    return SENSEI_STATUS_OK;
}

SENSEI_STATUS leo_scan_process(uint32_t pid, SENSEI_DETECTION_LIST *results) {
    if (!g_engine_initialized || !results) return SENSEI_STATUS_ERROR;
    return scan_one_process(pid, results);
}

/* Scoring utilities */
uint32_t leo_calculate_score(const SENSEI_DETECTION *detection) {
    if (!detection) return 0;
    uint32_t score = 0;
    switch (detection->priority) {
        case SENSEI_EVENT_PRIORITY_CRITICAL: score = 900; break;
        case SENSEI_EVENT_PRIORITY_HIGH:     score = 700; break;
        case SENSEI_EVENT_PRIORITY_MEDIUM:   score = 400; break;
        case SENSEI_EVENT_PRIORITY_LOW:      score = 100; break;
        default: score = 0;
    }
    score = (score * detection->confidence) / 100;
    if (score > SENSEI_THREAT_SCORE_MAX) score = SENSEI_THREAT_SCORE_MAX;
    return score;
}

SENSEI_MITRE_TECHNIQUE leo_map_mitre(const SENSEI_DETECTION *detection) {
    return detection ? detection->mitre_id : SENSEI_MITRE_NONE;
}

/* List management wrappers */
void leo_detection_list_free(SENSEI_DETECTION_LIST *list) {
    april_detection_list_free(list);
}

SENSEI_STATUS leo_detection_list_append(SENSEI_DETECTION_LIST *list,
                                        const SENSEI_DETECTION *detection) {
    return april_detection_list_append(list, detection);
}
