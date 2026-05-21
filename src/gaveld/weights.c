#include "weights.h"
#include <string.h>

/*
 * weights.c — Full signal taxonomy for gaveld scorer.
 *
 * Grouped by originating daemon. Lookup is linear scan — keep table
 * ordered by daemon for readability. Add new signals here only;
 * scorer.c and covariance matrix are separate.
 *
 * Weight scale:
 *   0–9   informational / telemetry
 *   10–19 low concern — worth tracking
 *   20–29 moderate — warrants attention
 *   30–39 high — likely malicious or misconfigured
 *   40–49 critical — strong indicator of compromise
 *
 * MITRE ATT&CK mobile technique IDs where applicable.
 */

static const weight_entry_t WEIGHT_TABLE[] = {

    /* ── burned — MIUI/HyperOS Identity & Privacy ───────────────────────── */

    /* HyperOS/MIUI fingerprint — informational */
    { "HYPEROS_DETECTED",               0, NULL     },  /* HyperOS vs MIUI — context signal */
    { "EEA_BUILD",                      0, NULL     },  /* EEA region — GDPR context */

    /* Privacy & tracking — baked into ROM */
    { "IMEI_EXPOSED",                  35, "T1592"  },  /* Both IMEIs readable via getprop */
    { "FACEBOOK_PARTNER_BAKED",        22, "T1195"  },  /* FB partner ID at system level */
    { "APPSFLYER_PREINSTALL",          20, "T1195"  },  /* AppsFlyer tracking preinstalled */
    { "PARTNER_BLOATWARE",             10, NULL     },  /* Spotify/Netflix/Google revenue deals */
    { "DEVICE_SERIAL_EXPOSED",         18, "T1592"  },  /* persist.sys.miui.sno readable */
    { "GOOGLE_CLIENT_XIAOMI",          12, "T1195"  },  /* Google clientidbase = android-xiaomi */

    /* Analytics & telemetry */
    { "MSA_TELEMETRY_ACTIVE",          22, "T1056"  },  /* MSA/MiSight analytics enabled */
    { "MISIGHT_ANALYTICS_ON",          18, "T1056"  },  /* misight.ue_mode != off */
    { "CLOUD_SYNC_ACTIVE",             12, NULL     },  /* persist.sys.cloud.enable = true */

    /* MIUI/HyperOS policy */
    { "MIUI_OPTIMIZATION_OFF",         10, NULL     },  /* persist.sys.miui_optimization = false */
    { "MIUI_OPTIMIZATIONS_OFF",         8, NULL     },  /* legacy alias — kept for compat */
    { "MIUI_RESTRICTED_MODE",          15, NULL     },  /* persist.sys.miui_restricted_mode = 1 */
    { "POWERKEEPER_ACTIVE",             8, NULL     },  /* persist.sys.powerkeeper = 1 */
    { "GAME_TURBO_ACTIVE",             10, NULL     },  /* persist.sys.game_turbo_enabled = 1 */
    { "CLEANER_AGGRESSIVE",            12, NULL     },  /* cleaner_level >= 2 */
    { "RAM_EXTENSION_ACTIVE",           8, NULL     },  /* memory_extension_enabled = 1 */
    { "DUAL_APPS_ACTIVE",              12, "T1078"  },  /* clone space active */
    { "MIUI_BOOSTER_RTMODE",            8, NULL     },  /* miuibooster.rtmode = true */
    { "PROCESS_KILL_AGGRESSIVE",       10, NULL     },  /* scout_binder_full_kill = true */
    { "APP_DOWNGRADE_ACTIVE",           8, NULL     },  /* downgrade_after_inactive_days set */
    { "SMART_GC_AGGRESSIVE",            8, NULL     },  /* smart_gc targeting system apps */

    { "MIUI_POLICY_ACTIVE",            18, NULL     },  /* invasive/privacy policies active */
    { "MIUI_PROPERTY_CHANGED",          15, "T1112"  },  /* tracked prop changed between scans */

    /* ── granitord — Security Posture & Boot Integrity ───────────────────── */

    { "VERIFIED_BOOT_FAIL",            30, "T1542"  },  /* ro.boot.verifiedbootstate != green */
    { "BOOTLOADER_UNLOCKED",           25, "T1542"  },  /* flash.locked != 1 */
    { "ROOT_DETECTED",                 35, "T1068"  },  /* su binary found */
    { "MAGISK_DETECTED",               35, "T1601"  },  /* Magisk/KernelSU paths found */
    { "SU_PRESENT",                    20, "T1068"  },  /* su in standard paths */
    { "DEBUGGABLE_BUILD",              15, "T1562"  },  /* ro.debuggable = 1 */
    { "ENCRYPTION_OFF",                30, "T1486"  },  /* ro.crypto.state != encrypted */
    { "ADB_ENABLED",                   25, "T1133"  },  /* persist.sys.usb.config contains adb */
    { "ADB_TCP_ENABLED",               32, "T1133"  },  /* ADB over TCP port 5555 open */
    { "CHINESE_SUPL_SERVER",           22, "T1071"  },  /* SUPL host = qxwz.com on EEA device */
    { "SELINUX_PERMISSIVE",            30, "T1562"  },  /* SELinux not enforcing */
    { "RAM_FAULT_REBOOT",              15, NULL     },  /* boot.reason.history = ehard* */
    { "FORCED_REBOOT_POLICY",           8, NULL     },  /* stability.reboot_days set */
    { "SECURITY_SCORE_LOW",            28, "T1562"  },  /* composite score below threshold */

    /* ── leatherheadd — Thermal Truth ───────────────────────────────────── */

    { "THERMAL_HAL_MISMATCH",          18, NULL     },  /* HAL vs cached delta > 5°C */
    { "THERMAL_WARN",                  12, NULL     },  /* skin temp > 42°C */
    { "THERMAL_CRITICAL",              22, NULL     },  /* skin temp > 48°C */
    { "CPU_THROTTLING",                15, NULL     },  /* cur_freq << max_freq sustained */
    { "SKIN_TEMP_HIGH",                15, NULL     },  /* real skin > cached skin */
    { "BATTERY_TEMP_HIGH",             18, NULL     },  /* battery temp > 40°C */
    { "THERMAL_STATUS_NONZERO",        20, NULL     },  /* Android thermal status > 0 */

    /* ── rocksteadyd — CPU Load & Frequency ─────────────────────────────── */

    { "CPU_HOG",                       18, "T1641"  },  /* single process > HOG_PCT */
    { "CPU_HOG_CRITICAL",              28, "T1641"  },  /* single process > CRITICAL_PCT */
    { "CPU_CLUSTER_IMBALANCE",         12, NULL     },  /* efficiency > performance cluster */
    { "GOVERNOR_PERFORMANCE",           8, NULL     },  /* governor locked to performance */
    { "ALL_CORES_MAXED",               20, "T1641"  },  /* all cores at max freq sustained */

    /* ── bebopd — Wakelocks & Power ─────────────────────────────────────── */

    { "WAKELOCK_ANOMALY",              20, "T1641"  },  /* unexpected wakelock pattern */
    { "WAKELOCK_FULL_HELD",            25, "T1641"  },  /* full wakelock held by non-system */
    { "WAKELOCK_DRAIN_HIGH",           20, "T1641"  },  /* interactive drain > 400mAh/h */
    { "DOZE_INTERRUPTED",              15, NULL     },  /* doze repeatedly broken */
    { "INCALL_WAKELOCK_ORPHAN",        22, "T1641"  },  /* InCall wakelock without active call */
    { "BATTERY_LEVEL_CRITICAL",        18, NULL     },  /* battery < 10% */
    { "BATTERY_SAVER_OFF_LOW",         10, NULL     },  /* battery low but saver disabled */

    /* ── rahzerd — Network & Connectivity ───────────────────────────────── */

    { "EXCESSIVE_CONNECTIONS",         25, "T1071"  },
    { "CONNECTION_SPIKE",              20, "T1071"  },
    { "UNKNOWN_LISTENER",              28, "T1049"  },
    { "DNS_ANOMALY",                   22, "T1071"  },
    { "CLEARTEXT_EXFIL",               30, "T1048"  },
    { "WIFI_BSSID_CHANGE",             18, "T1557"  },
    { "VPN_ACTIVE",                    10, NULL     },
    { "ROAMING_DATA_ACTIVE",            8, NULL     },
    { "DUAL_SIM_ACTIVE",                5, NULL     },  /* informational */
    { "NO_DNS_RESOLUTION",             20, "T1071"  },  /* DNS failure */
    { "PRIVATE_DNS_INACTIVE",          12, NULL     },  /* no DoT/DoH configured */

    /* ── ratkingd — Process & Memory ────────────────────────────────────── */

    { "ZOMBIE_DETECTED",               18, "T1014"  },  /* zombie processes present */
    { "MEM_PRESSURE",                  20, "T1499"  },  /* MemAvailable below threshold */
    { "RAM_CRITICAL",                  25, "T1499"  },  /* MemAvailable critically low */
    { "SWAP_HIGH",                     15, NULL     },  /* swap usage high */
    { "LMK_KILL_RATE_HIGH",            15, NULL     },  /* LMK killing frequently */
    { "BG_APP_LIMIT_HIGH",              8, NULL     },  /* bg_apps_limit unusually high */
    { "CPU_HOG_PROCESS",               18, "T1641"  },  /* specific process CPU hog */

    /* ── metalheadd — Sensor Registry & Access ───────────────────────────── */

    { "SENSOR_ACCESS_ANOMALY",         22, "T1056"  },  /* background app on sensitive sensor */
    { "SENSOR_FLOOD",                  18, "T1641"  },  /* sampling rate abnormally high */
    { "SENSITIVE_SENSOR_ACTIVE",       20, "T1056"  },  /* gyro/accel/mic with many clients */
    { "SENSOR_CLIENT_SPIKE",           15, "T1056"  },  /* unusual client count on sensor */

    /* ── shredderd — Kernel Integrity & Root Persistence ────────────────── */

    { "KERNEL_MODULE_UNKNOWN",         35, "T1014"  },  /* non-stock kernel module loaded */
    { "DEBUGFS_MOUNTED",               20, "T1014"  },  /* debugfs accessible */
    { "ROOT_PROCESS_COUNT_HIGH",       25, "T1068"  },  /* many uid=0 processes */
    { "INTEGRITY_SCORE_LOW",           30, "T1565"  },  /* composite integrity below threshold */
    { "ROOT_PERSISTENCE",              38, "T1601"  },  /* root found across consecutive polls */
    { "ROOTKIT_MODULE",                40, "T1014"  },
    { "INTEGRITY_VIOLATION",           45, "T1565"  },
    { "RWX_MEMORY_PAGE",               35, "T1055"  },
    { "REFLECTIVE_CODE",               35, "T1055"  },
    { "HIDDEN_PROCESS",                38, "T1014"  },
    { "SUSPICIOUS_PATH",               25, "T1036"  },
    { "UNUSUAL_PARENT",                22, "T1055"  },

    /* ── fugitoidd — Foreground Activity & System Events ────────────────── */

    { "ANR_DETECTED",                  15, NULL     },  /* ANR in logcat */
    { "CRASH_DETECTED",                18, NULL     },  /* FATAL EXCEPTION in logcat */
    { "OOM_KILL_EVENT",                20, "T1499"  },  /* lowmemorykiller event */
    { "APP_SWITCH_ANOMALY",            10, NULL     },  /* rapid foreground app switching */
    { "WATCHDOG_TRIGGER",              22, NULL     },  /* watchdog event in logcat */
    { "SERVICE_COUNT_HIGH",            10, NULL     },  /* unusual service count */

    /* ── Casey (Hooks / Kernel) ──────────────────────────────────────────── */

    { "ANON_EXEC_MEM",                 40, "T1055"  },
    { "SYS_TABLE_EXPOSED",             40, "T1014"  },
    { "KPTR_LEAK",                     30, "T1014"  },
    { "LD_PRELOAD",                    30, "T1574"  },
    { "MIUI_SCRAPER",                  25, "T1056"  },

    /* ── Leo (Orchestrator) ──────────────────────────────────────────────── */

    { "MIUI_MQSAS_ACTIVE",             18, "T1056"  },  /* MIUI quality analytics service */
    { "MIGARD_ACTIVE",                 12, NULL     },  /* MIUI security guard running */
    { "PERFSHIELDER_ACTIVE",            8, NULL     },  /* performance shield engaged */

    /* ── Superhero binary ────────────────────────────────────────────────── */

    { "BATTERY_HEALTH_LOW",            20, NULL     },
    { "OLD_KERNEL",                    15, NULL     },
    { "BACKGROUND_RESTRICTED_APPS",    10, NULL     },
};

#define TABLE_SIZE ((int)(sizeof(WEIGHT_TABLE) / sizeof(WEIGHT_TABLE[0])))

int weight_lookup(const char *signal) {
    if (!signal) return 0;
    for (int i = 0; i < TABLE_SIZE; i++)
        if (strcmp(WEIGHT_TABLE[i].signal, signal) == 0)
            return WEIGHT_TABLE[i].base_weight;
    return 0;
}

const char *weight_mitre(const char *signal) {
    if (!signal) return NULL;
    for (int i = 0; i < TABLE_SIZE; i++)
        if (strcmp(WEIGHT_TABLE[i].signal, signal) == 0)
            return WEIGHT_TABLE[i].mitre_id;
    return NULL;
}

int weight_table_size(void) { return TABLE_SIZE; }
