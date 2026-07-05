/*
 * sar_engine.c — GDPR Subject Access Request Engine
 *
 * Handles: inventory, export, purge, consent management
 * for MiuiserPeruser data stores.
 *
 * Legal note: MiuiserPeruser processes data about THIS DEVICE ONLY,
 * by the device owner, for personal security research.
 * Falls under GDPR Article 2(2)(c) household exemption.
 * SAR implemented for transparency and user control.
 *
 * Usage:
 *   sar_engine --inventory
 *   sar_engine --export --out /sdcard/MiuiserPeruser_SAR.tar.gz
 *   sar_engine --purge [--category daemon_results|logs|baselines|all]
 *   sar_engine --consent --list
 *   sar_engine --consent --revoke [syndicate|superhero|all]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>

#define BASE    "/data/data/com.termux/files/home/MiuiserPeruser"
#define VERSION "1.0"

/* ── Data store categories ────────────────────────────────────── */
typedef struct {
    const char *path;
    const char *category;
    const char *description;
} DataStore;

static const DataStore STORES[] = {
    { BASE "/Registry/daemon_results", "daemon_results",
      "Real-time daemon scan results (JSON)" },
    { BASE "/Registry/daemon_state.json", "registry",
      "Daemon state registry" },
    { BASE "/Registry/daemon_allowlist.json", "registry",
      "Daemon allowlist configuration" },
    { BASE "/Registry/system_registry.json", "registry",
      "System registry" },
    { BASE "/data/granitord_baseline.json", "baseline",
      "Security posture baseline" },
    { BASE "/data/shredderd_baseline.json", "baseline",
      "Kernel integrity baseline" },
    { BASE "/data/tigerclawd_baseline.json", "baseline",
      "HyperOS binder topology baseline" },
    { BASE "/data/nulld_baseline.json", "baseline",
      "Idle transmission baseline" },
    { BASE "/data/rahzerd_baseline.json", "baseline",
      "Network connectivity baseline" },
    { BASE "/data/syndicate_config.json", "config",
      "Syndicate daemon configuration" },
    { BASE "/data/last_scan.json", "config",
      "Last scan metadata" },
    { BASE "/data/profiles/peruse_profile.json", "profile",
      "Device profile data" },
    { BASE "/data/exodus_trackers.csv", "reference",
      "Exodus tracker database (reference data)" },
    { BASE "/data/ssl_ja3.csv", "reference",
      "SSL JA3 fingerprint database (reference data)" },
    { BASE "/data/.syndicate_consent.lock", "consent",
      "Syndicate scan consent record" },
    { BASE "/data/.sensei_consent.lock", "consent",
      "Superhero scan consent record" },
    { BASE "/data/training_complete.lock", "consent",
      "Training completion record" },
    { BASE "/logs", "logs",
      "Daemon operation logs" },
    { BASE "/data/april.bin", "database",
      "April detection database" },
    { BASE "/data/superhero.db", "database",
      "Superhero scan database" },
    { NULL, NULL, NULL }
};

/* ── Helpers ──────────────────────────────────────────────────── */
static long file_size(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (long)st.st_size;
}

static time_t file_mtime(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return st.st_mtime;
}

static long dir_size(const char *path) {
    DIR *d = opendir(path);
    if (!d) return 0;
    long total = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char full[512];
        snprintf(full, sizeof(full), "%s/%s", path, e->d_name);
        struct stat st;
        if (stat(full, &st) == 0) total += st.st_size;
    }
    closedir(d);
    return total;
}

static int dir_count(const char *path) {
    DIR *d = opendir(path);
    if (!d) return 0;
    int count = 0;
    struct dirent *e;
    while ((e = readdir(d)))
        if (e->d_name[0] != '.') count++;
    closedir(d);
    return count;
}

static void human_size(long bytes, char *out, size_t outlen) {
    if (bytes < 1024)           snprintf(out, outlen, "%ldB", bytes);
    else if (bytes < 1048576)   snprintf(out, outlen, "%.1fKB", bytes/1024.0);
    else                        snprintf(out, outlen, "%.1fMB", bytes/1048576.0);
}

static void format_time(time_t t, char *out, size_t outlen) {
    if (t == 0) { strncpy(out, "never", outlen); return; }
    strftime(out, outlen, "%Y-%m-%d %H:%M", localtime(&t));
}

/* ── Inventory ────────────────────────────────────────────────── */
static void cmd_inventory(void) {
    printf("\n╔══════════════════════════════════════════════════╗\n");
    printf("║  MIUISERPERUSER — DATA INVENTORY                ║\n");
    printf("║  GDPR Article 2(2)(c) — Personal Research Use  ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");

    printf("  Device processes data about ITSELF ONLY.\n");
    printf("  Data subject = Data controller = Device owner.\n");
    printf("  No data transmitted to third parties.\n\n");

    printf("  %-20s %-12s %-16s %s\n",
           "CATEGORY", "SIZE", "LAST MODIFIED", "DESCRIPTION");
    printf("  %s\n", "────────────────────────────────────────────────");

    long total = 0;
    for (int i = 0; STORES[i].path; i++) {
        struct stat st;
        if (stat(STORES[i].path, &st) != 0) continue;

        long sz;
        time_t mt;
        char size_str[32], time_str[32];

        if (S_ISDIR(st.st_mode)) {
            sz = dir_size(STORES[i].path);
            mt = st.st_mtime;
            int cnt = dir_count(STORES[i].path);
            snprintf(size_str, sizeof(size_str), "");
            human_size(sz, size_str, sizeof(size_str));
            char cnt_str[64];
            snprintf(cnt_str, sizeof(cnt_str), "%s (%d files)", size_str, cnt);
            format_time(mt, time_str, sizeof(time_str));
            printf("  %-20s %-12s %-16s %s\n",
                   STORES[i].category, cnt_str, time_str,
                   STORES[i].description);
        } else {
            sz = file_size(STORES[i].path);
            mt = file_mtime(STORES[i].path);
            human_size(sz, size_str, sizeof(size_str));
            format_time(mt, time_str, sizeof(time_str));
            printf("  %-20s %-12s %-16s %s\n",
                   STORES[i].category, size_str, time_str,
                   STORES[i].description);
        }
        if (sz > 0) total += sz;
    }

    char total_str[32];
    human_size(total, total_str, sizeof(total_str));
    printf("  %s\n", "────────────────────────────────────────────────");
    printf("  %-20s %-12s\n\n", "TOTAL", total_str);

    /* Consent status */
    printf("  CONSENT STATUS:\n");
    printf("  Syndicate scan: %s\n",
           access(BASE "/data/.syndicate_consent.lock", F_OK) == 0
           ? "CONSENTED" : "NOT CONSENTED");
    printf("  Superhero scan: %s\n\n",
           access(BASE "/data/.sensei_consent.lock", F_OK) == 0
           ? "CONSENTED" : "NOT CONSENTED");
}

/* ── Export ───────────────────────────────────────────────────── */
static void cmd_export(const char *out_path) {
    char ts[32];
    time_t t = time(NULL);
    strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", localtime(&t));

    char default_path[256];
    if (!out_path) {
        snprintf(default_path, sizeof(default_path),
                 "/sdcard/MiuiserPeruser_SAR_%s.tar.gz", ts);
        out_path = default_path;
    }

    printf("\n  Exporting SAR to: %s\n", out_path);

    /* Build tar command */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "cd '%s' && tar czf '%s' "
        "Registry/daemon_results/*.json "
        "Registry/daemon_state.json "
        "data/syndicate_config.json "
        "data/granitord_baseline.json "
        "data/shredderd_baseline.json "
        "data/last_scan.json "
        "logs/*.log "
        "2>/dev/null",
        BASE, out_path);

    int ret = system(cmd);
    if (ret == 0) {
        long sz = file_size(out_path);
        char size_str[32];
        human_size(sz, size_str, sizeof(size_str));
        printf("  ✔ Export complete: %s (%s)\n", out_path, size_str);
        printf("\n  This archive contains all data collected by\n");
        printf("  MiuiserPeruser about this device. It is your\n");
        printf("  right to access, share, or delete this data.\n\n");
    } else {
        printf("  ✗ Export failed\n\n");
    }
}

/* ── Purge ────────────────────────────────────────────────────── */
static void purge_dir(const char *path) {
    DIR *d = opendir(path);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char full[512];
        snprintf(full, sizeof(full), "%s/%s", path, e->d_name);
        unlink(full);
    }
    closedir(d);
}

static void cmd_purge(const char *category) {
    printf("\n  ⚠ PURGE OPERATION\n\n");
    printf("  This will permanently delete collected data.\n");
    printf("  Type DELETE to confirm: ");

    char confirm[32] = {0};
    if (!fgets(confirm, sizeof(confirm), stdin)) return;
    confirm[strcspn(confirm, "\n")] = 0;

    if (strcmp(confirm, "DELETE") != 0) {
        printf("\n  Purge cancelled.\n\n");
        return;
    }

    int all = (!category || strcmp(category, "all") == 0);
    int purged = 0;

    if (all || strcmp(category, "daemon_results") == 0) {
        purge_dir(BASE "/Registry/daemon_results");
        printf("  ✔ Daemon results purged\n");
        purged++;
    }
    if (all || strcmp(category, "logs") == 0) {
        purge_dir(BASE "/logs");
        printf("  ✔ Logs purged\n");
        purged++;
    }
    if (all || strcmp(category, "baselines") == 0) {
        unlink(BASE "/data/granitord_baseline.json");
        unlink(BASE "/data/shredderd_baseline.json");
        unlink(BASE "/data/tigerclawd_baseline.json");
        unlink(BASE "/data/nulld_baseline.json");
        unlink(BASE "/data/rahzerd_baseline.json");
        printf("  ✔ Baselines purged\n");
        purged++;
    }
    if (all || strcmp(category, "consent") == 0) {
        unlink(BASE "/data/.syndicate_consent.lock");
        unlink(BASE "/data/.sensei_consent.lock");
        unlink(BASE "/data/training_complete.lock");
        printf("  ✔ Consent records purged\n");
        purged++;
    }
    if (all) {
        unlink(BASE "/data/last_scan.json");
        printf("  ✔ Scan metadata purged\n");
        purged++;
    }

    printf("\n  %d categor%s purged.\n", purged, purged == 1 ? "y" : "ies");
    printf("  All consent must be re-given before next scan.\n\n");
}

/* ── Consent ──────────────────────────────────────────────────── */
static void cmd_consent_list(void) {
    printf("\n  CONSENT RECORDS:\n\n");
    printf("  %-20s %-12s %s\n", "SUBSYSTEM", "STATUS", "LOCK FILE");
    printf("  %s\n", "──────────────────────────────────────────");

    struct { const char *name; const char *lock; } consents[] = {
        { "Syndicate scan", BASE "/data/.syndicate_consent.lock" },
        { "Superhero scan", BASE "/data/.sensei_consent.lock" },
        { "Training",       BASE "/data/training_complete.lock" },
        { NULL, NULL }
    };

    for (int i = 0; consents[i].name; i++) {
        int active = (access(consents[i].lock, F_OK) == 0);
        char ts[32] = "n/a";
        if (active) {
            time_t mt = file_mtime(consents[i].lock);
            format_time(mt, ts, sizeof(ts));
        }
        printf("  %-20s %-12s %s\n",
               consents[i].name,
               active ? "CONSENTED" : "NOT SET",
               active ? ts : "");
    }
    printf("\n  Under GDPR Article 2(2)(c), consent is given\n");
    printf("  by the device owner to process data about\n");
    printf("  their own device for personal security research.\n\n");
}

static void cmd_consent_revoke(const char *subsystem) {
    printf("\n  Revoking consent");
    if (subsystem) printf(" for: %s", subsystem);
    printf("\n  Type REVOKE to confirm: ");

    char confirm[32] = {0};
    if (!fgets(confirm, sizeof(confirm), stdin)) return;
    confirm[strcspn(confirm, "\n")] = 0;

    if (strcmp(confirm, "REVOKE") != 0) {
        printf("\n  Revocation cancelled.\n\n");
        return;
    }

    int all = (!subsystem || strcmp(subsystem, "all") == 0);

    if (all || strstr(subsystem, "syndicate")) {
        unlink(BASE "/data/.syndicate_consent.lock");
        printf("  ✔ Syndicate consent revoked\n");
    }
    if (all || strstr(subsystem, "superhero")) {
        unlink(BASE "/data/.sensei_consent.lock");
        printf("  ✔ Superhero consent revoked\n");
    }
    printf("\n  Consent revoked. Re-run the relevant scan\n");
    printf("  to provide consent again.\n\n");
}

/* ── Main ─────────────────────────────────────────────────────── */
int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("MiuiserPeruser SAR Engine v%s\n\n", VERSION);
        printf("Usage:\n");
        printf("  sar_engine --inventory\n");
        printf("  sar_engine --export [--out /path/to/output.tar.gz]\n");
        printf("  sar_engine --purge [--category daemon_results|logs|baselines|consent|all]\n");
        printf("  sar_engine --consent --list\n");
        printf("  sar_engine --consent --revoke [syndicate|superhero|all]\n\n");
        return 0;
    }

    const char *cmd = argv[1];
    const char *out_path = NULL;
    const char *category = NULL;
    const char *subsystem = NULL;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--out") == 0 && i+1 < argc)
            out_path = argv[++i];
        else if (strcmp(argv[i], "--category") == 0 && i+1 < argc)
            category = argv[++i];
        else if (strcmp(argv[i], "--revoke") == 0)
            subsystem = (i+1 < argc && argv[i+1][0] != '-') ? argv[++i] : "all";
        else if (strcmp(argv[i], "--list") == 0)
            category = "list";
    }

    if (strcmp(cmd, "--inventory") == 0)
        cmd_inventory();
    else if (strcmp(cmd, "--export") == 0)
        cmd_export(out_path);
    else if (strcmp(cmd, "--purge") == 0)
        cmd_purge(category);
    else if (strcmp(cmd, "--consent") == 0) {
        if (category && strcmp(category, "list") == 0)
            cmd_consent_list();
        else if (subsystem)
            cmd_consent_revoke(subsystem);
        else
            cmd_consent_list();
    } else {
        printf("Unknown command: %s\n", cmd);
        return 1;
    }

    return 0;
}
