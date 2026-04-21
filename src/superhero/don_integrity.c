/*
 * MiuiserPeruser – File integrity monitor (Donatello)
 */

#include <leo_detection.h>
#include <april_platform.h>
#include <sensei_types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <openssl/evp.h>

#define CACHE_FILE "/data/data/com.termux/files/home/.miuiserperuser_integrity_cache"
#define MAX_FILES 16

extern void april_log(const char* level, const char* format, ...);

/* Critical Termux files to monitor */
static const char *critical_paths[] = {
    "/data/data/com.termux/files/home/.bashrc",
    "/data/data/com.termux/files/usr/bin/bash",
    "/data/data/com.termux/files/usr/bin/login",
    "/data/data/com.termux/files/usr/lib/libtermux-exec.so",
    NULL
};

typedef struct {
    char path[256];
    time_t mtime;
    char hash[65]; // 64 hex + null
} file_record_t;

static file_record_t cache[MAX_FILES];
static int cache_count = 0;

static void load_cache(void) {
    FILE *f = fopen(CACHE_FILE, "r");
    if (!f) return;
    char line[512];
    while (fgets(line, sizeof(line), f) && cache_count < MAX_FILES) {
        file_record_t *rec = &cache[cache_count];
        char mtime_str[32];
        if (sscanf(line, "%255s %31s %64s", rec->path, mtime_str, rec->hash) == 3) {
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

static int compute_sha256(const char *path, char *output) {
    FILE *file = fopen(path, "rb");
    if (!file) return -1;

    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        fclose(file);
        return -1;
    }

    const EVP_MD *md = EVP_sha256();
    unsigned char buffer[8192];
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;
    size_t bytes;

    EVP_DigestInit_ex(mdctx, md, NULL);
    while ((bytes = fread(buffer, 1, sizeof(buffer), file))) {
        EVP_DigestUpdate(mdctx, buffer, bytes);
    }
    EVP_DigestFinal_ex(mdctx, hash, &hash_len);
    EVP_MD_CTX_free(mdctx);
    fclose(file);

    for (unsigned int i = 0; i < hash_len; i++) {
        sprintf(output + (i * 2), "%02x", hash[i]);
    }
    output[hash_len * 2] = '\0';
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
        struct stat st;
        if (stat(critical_paths[i], &st) != 0) {
            april_log("THREAT", "INTEGRITY: File missing: %s", critical_paths[i]);
            continue;
        }

        file_record_t *rec = NULL;
        for (int j = 0; j < cache_count; j++) {
            if (strcmp(cache[j].path, critical_paths[i]) == 0) {
                rec = &cache[j];
                break;
            }
        }

        char current_hash[65];
        if (compute_sha256(critical_paths[i], current_hash) != 0) {
            april_log("WARN", "INTEGRITY: Cannot read file: %s", critical_paths[i]);
            continue;
        }

        if (!rec) {
            if (cache_count < MAX_FILES) {
                rec = &cache[cache_count++];
                strcpy(rec->path, critical_paths[i]);
                rec->mtime = st.st_mtime;
                strcpy(rec->hash, current_hash);
                april_log("INFO", "INTEGRITY: Baseline recorded: %s", critical_paths[i]);
            }
        } else {
            if (rec->mtime != st.st_mtime) {
                if (strcmp(rec->hash, current_hash) != 0) {
                    april_log("THREAT", "INTEGRITY: File changed: %s", critical_paths[i]);
                    april_log("DEBUG", "  Old hash: %s", rec->hash);
                    april_log("DEBUG", "  New hash: %s", current_hash);
                    rec->mtime = st.st_mtime;
                    strcpy(rec->hash, current_hash);
                } else {
                    april_log("INFO", "INTEGRITY: Timestamp updated (no content change): %s", critical_paths[i]);
                    rec->mtime = st.st_mtime;
                }
            }
        }
    }

    save_cache();
    return SENSEI_STATUS_OK;
}
