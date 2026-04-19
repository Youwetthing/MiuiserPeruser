#include "compat/sensei_compat.h"
/*
 * MiuiserPeruser – Full System File Integrity Monitor (Donatello)
 * Monitors critical Android system files using Shizuku/RISH.
 */

#include <leo_detection.h>
#include <sensei_types.h>
#include "rish_pipe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <openssl/evp.h>

#define CACHE_FILE "/data/data/com.termux/files/home/.miuiserperuser_integrity_cache"
#define MAX_FILES 64

extern void april_log(const char* level, const char* format, ...);

/* Critical system files to monitor (full Android coverage) */
static const char *critical_paths[] = {
    // Core Android init and daemons
    "/system/bin/init",
    "/system/bin/zygote",
    "/system/bin/zygote64",
    "/system/bin/surfaceflinger",
    "/system/bin/logd",
    "/system/bin/vold",
    "/system/bin/adbd",
    "/system/bin/servicemanager",
    "/system/bin/bootanimation",
    "/system/bin/netd",
    "/system/bin/installd",
    "/system/bin/keystore",
    "/system/bin/mediaserver",
    "/system/bin/drmserver",
    "/system/bin/gatekeeperd",
    
    // Critical native libraries
    "/system/lib/libc.so",
    "/system/lib64/libc.so",
    "/system/lib/libm.so",
    "/system/lib64/libm.so",
    "/system/lib/libdl.so",
    "/system/lib64/libdl.so",
    "/system/lib/libsqlite.so",
    "/system/lib64/libsqlite.so",
    "/system/lib/libandroid_runtime.so",
    "/system/lib64/libandroid_runtime.so",
    "/system/lib/libbinder.so",
    "/system/lib64/libbinder.so",
    
    // Security and configuration
    "/system/build.prop",
    "/system/etc/hosts",
    "/system/etc/security/permissions/platform.xml",
    
    // Termux self-protection (kept)
    "/data/data/com.termux/files/home/.bashrc",
    "/data/data/com.termux/files/usr/bin/bash",
    "/data/data/com.termux/files/usr/bin/login",
    "/data/data/com.termux/files/usr/lib/libtermux-exec.so",
    
    // MIUI/Xiaomi specific
    "/system/bin/mi_thermald",
    "/system/bin/miui-booster",
    "/system/bin/xiaomi-security",
    
    NULL
};

typedef struct {
    char path[512];
    time_t mtime;
    char hash[65]; // SHA-256 hex
} file_record_t;

static file_record_t cache[MAX_FILES];
static int cache_count = 0;

static void load_cache(void) {
    FILE *f = fopen(CACHE_FILE, "r");
    if (!f) return;
    char line[1024];
    while (fgets(line, sizeof(line), f) && cache_count < MAX_FILES) {
        file_record_t *rec = &cache[cache_count];
        char mtime_str[32];
        if (sscanf(line, "%511s %31s %64s", rec->path, mtime_str, rec->hash) == 3) {
            rec->mtime = atol(mtime_str);
            cache_count++;
        }
    }
    fclose(f);
}

static void save_cache(void) {
    FILE *f = fopen(CACHE_FILE, "w");
    if (!f) return;
    for (int i = 0; i < cache_count; i++) {
        fprintf(f, "%s %ld %s\n", cache[i].path, cache[i].mtime, cache[i].hash);
    }
    fclose(f);
}

static int compute_sha256_via_rish(const char *path, char *output) {
    // Use RISH to cat the file and pipe to sha256sum
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "cat \"%s\" 2>/dev/null | sha256sum", path);
    char *result = rish_pipe_command(cmd);
    if (!result || strlen(result) < 64) {
        free(result);
        return -1;
    }
    strncpy(output, result, 64);
    output[64] = '\0';
    free(result);
    return 0;
}

SENSEI_STATUS don_integrity_check(SENSEI_DETECTION_LIST *results) {
    (void)results;
    static int cache_loaded = 0;
    
    if (!cache_loaded) {
        load_cache();
        cache_loaded = 1;
    }

    for (int i = 0; critical_paths[i]; i++) {
        // Check if file exists via RISH
        char test_cmd[512];
        snprintf(test_cmd, sizeof(test_cmd), "test -f \"%s\" && echo yes", critical_paths[i]);
        char *exists = rish_pipe_command(test_cmd);
        if (!exists || strstr(exists, "yes") == NULL) {
            free(exists);
            continue; // File doesn't exist on this device, skip
        }
        free(exists);

        file_record_t *rec = NULL;
        for (int j = 0; j < cache_count; j++) {
            if (strcmp(cache[j].path, critical_paths[i]) == 0) {
                rec = &cache[j];
                break;
            }
        }

        char current_hash[65];
        if (compute_sha256_via_rish(critical_paths[i], current_hash) != 0) {
            april_log("WARN", "INTEGRITY: Cannot read file: %s", critical_paths[i]);
            continue;
        }

        // Get mtime via RISH
        char mtime_cmd[512];
        snprintf(mtime_cmd, sizeof(mtime_cmd), "stat -c %%Y \"%s\" 2>/dev/null", critical_paths[i]);
        char *mtime_str = rish_pipe_command(mtime_cmd);
        time_t current_mtime = mtime_str ? atol(mtime_str) : 0;
        free(mtime_str);

        if (!rec) {
            // New file, add to cache
            if (cache_count < MAX_FILES) {
                rec = &cache[cache_count++];
                strcpy(rec->path, critical_paths[i]);
                rec->mtime = current_mtime;
                strcpy(rec->hash, current_hash);
                april_log("INFO", "INTEGRITY: Baseline recorded: %s", critical_paths[i]);
            }
        } else {
            // Compare
            if (strcmp(rec->hash, current_hash) != 0) {
                april_log("THREAT", "INTEGRITY: File changed: %s", critical_paths[i]);
                april_log("DEBUG", "  Old hash: %s", rec->hash);
                april_log("DEBUG", "  New hash: %s", current_hash);
                rec->mtime = current_mtime;
                strcpy(rec->hash, current_hash);
            } else if (rec->mtime != current_mtime) {
                april_log("INFO", "INTEGRITY: Timestamp updated (no content change): %s", critical_paths[i]);
                rec->mtime = current_mtime;
            }
        }
    }

    save_cache();
    return SENSEI_STATUS_OK;
}
