#include "compat/sensei_compat.h"
#include "leo_detection.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sqlite3.h>
#include <stdarg.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>

#define LOG_PREFIX   "[LEO]"
#ifndef BASE_PATH
#define BASE_PATH    "/data/data/com.termux/files/home/MiuiserPeruser"
#endif
#define DB_PATH      BASE_PATH "/data/superhero.db"
#define PROFILE_PATH BASE_PATH "/data/profiles/behavioural_baseline.json"
#define SAR_PATH     BASE_PATH "/data/SAR_export.json"
#define BASELINE_CYCLES 12
#define RETENTION_DAYS  90

static sqlite3 *db = NULL;
static volatile int running = 1;
static int cycle = 0;
static int verbose_mode = 0;

/* ── Logging ──────────────────────────────────────────────────────────────── */
void april_log(const char *level, const char *format, ...) {
    va_list args;
    va_start(args, format);
    printf("%s [%s] ", LOG_PREFIX, level);
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

void april_log_verbose(const char *module, const char *check, const char *result) {
    if (!verbose_mode) return;
    printf("%s [VERBOSE] [%s] %s → %s\n", LOG_PREFIX, module, check, result);
}

void leo_set_verbose(int v) { verbose_mode = v; }

/* ── SQLite helpers ───────────────────────────────────────────────────────── */
static void db_exec(const char *sql) {
    char *err = NULL;
    sqlite3_exec(db, sql, NULL, NULL, &err);
    if (err) { sqlite3_free(err); }
}

static void create_schema(void) {
    db_exec(
        "CREATE TABLE IF NOT EXISTS detections ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "scan_id INTEGER,"
        "timestamp INTEGER DEFAULT (strftime('%s','now')),"
        "turtle TEXT,"
        "type TEXT,"
        "description TEXT,"
        "priority TEXT,"
        "confidence INTEGER,"
        "is_baseline INTEGER DEFAULT 0"
        ");"
    );
    db_exec(
        "CREATE TABLE IF NOT EXISTS scan_history ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "timestamp INTEGER DEFAULT (strftime('%s','now')),"
        "depth TEXT,"
        "finding_count INTEGER,"
        "new_findings INTEGER DEFAULT 0,"
        "resolved_findings INTEGER DEFAULT 0,"
        "threat_level INTEGER DEFAULT 0,"
        "thermal INTEGER,"
        "battery INTEGER,"
        "cpu_freq INTEGER,"
        "is_baseline INTEGER DEFAULT 0"
        ");"
    );
    db_exec(
        "CREATE TABLE IF NOT EXISTS behavioural_baseline ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "established_at INTEGER DEFAULT (strftime('%s','now')),"
        "scan_count INTEGER,"
        "normal_finding_types TEXT,"
        "normal_finding_count_avg REAL,"
        "known_modules TEXT,"
        "known_processes TEXT,"
        "device_sno TEXT,"
        "device_fb_id TEXT"
        ");"
    );
    db_exec(
        "CREATE TABLE IF NOT EXISTS gdpr_log ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "timestamp INTEGER DEFAULT (strftime('%s','now')),"
        "action TEXT,"
        "detail TEXT"
        ");"
    );
    /* Retention: purge detections older than 90 days */
    db_exec(
        "DELETE FROM detections WHERE timestamp < strftime('%s','now') - 7776000;"
    );
    db_exec(
        "DELETE FROM scan_history WHERE timestamp < strftime('%s','now') - 7776000;"
    );
}

/* ── Detection writer ─────────────────────────────────────────────────────── */
static int current_scan_id = 0;

void leo_write_detection(const char *turtle, const char *type,
                         const char *description, const char *priority,
                         int confidence) {
    if (!db) return;
    const char *sql =
        "INSERT INTO detections (scan_id, turtle, type, description, priority, confidence) "
        "VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int (stmt, 1, current_scan_id);
        sqlite3_bind_text(stmt, 2, turtle,      -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, type,        -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, description, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, priority,    -1, SQLITE_STATIC);
        sqlite3_bind_int (stmt, 6, confidence);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    april_log_verbose(turtle, type, description);
}

/* ── Scan history writer ──────────────────────────────────────────────────── */
static int write_scan_history(const char *depth, int finding_count,
                               int new_findings, int thermal,
                               int battery, int cpu_freq) {
    const char *sql =
        "INSERT INTO scan_history "
        "(depth, finding_count, new_findings, thermal, battery, cpu_freq) "
        "VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt *stmt;
    int id = 0;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, depth,         -1, SQLITE_STATIC);
        sqlite3_bind_int (stmt, 2, finding_count);
        sqlite3_bind_int (stmt, 3, new_findings);
        sqlite3_bind_int (stmt, 4, thermal);
        sqlite3_bind_int (stmt, 5, battery);
        sqlite3_bind_int (stmt, 6, cpu_freq);
        sqlite3_step(stmt);
        id = (int)sqlite3_last_insert_rowid(db);
        sqlite3_finalize(stmt);
    }
    return id;
}

/* ── Baseline management ──────────────────────────────────────────────────── */
static int baseline_exists(void) {
    sqlite3_stmt *stmt;
    int exists = 0;
    if (sqlite3_prepare_v2(db,
            "SELECT COUNT(*) FROM behavioural_baseline;",
            -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW)
            exists = sqlite3_column_int(stmt, 0) > 0;
        sqlite3_finalize(stmt);
    }
    return exists;
}

static void establish_baseline(int scan_count) {
    /* Collect normal finding types from last N scans */
    sqlite3_stmt *stmt;
    char normal_types[2048] = {0};
    if (sqlite3_prepare_v2(db,
            "SELECT DISTINCT type FROM detections ORDER BY type;",
            -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *t = (const char*)sqlite3_column_text(stmt, 0);
            if (t) { size_t rem = sizeof(normal_types)-strlen(normal_types)-2; strncat(normal_types, t, rem); strncat(normal_types, ",", 1); normal_types[sizeof(normal_types)-1] = '\0'; }
        }
        sqlite3_finalize(stmt);
    }

    /* Average finding count */
    double avg = 0;
    if (sqlite3_prepare_v2(db,
            "SELECT AVG(finding_count) FROM scan_history;",
            -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) avg = sqlite3_column_double(stmt, 0);
        sqlite3_finalize(stmt);
    }

    const char *sql =
        "INSERT INTO behavioural_baseline "
        "(scan_count, normal_finding_types, normal_finding_count_avg) "
        "VALUES (?, ?, ?);";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int   (stmt, 1, scan_count);
        sqlite3_bind_text  (stmt, 2, normal_types, -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 3, avg);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    db_exec("INSERT INTO gdpr_log (action, detail) VALUES "
            "('BASELINE_ESTABLISHED', 'Behavioural baseline created from scan history');");
    april_log("INFO", "LEO: Behavioural baseline established from %d scans", scan_count);
}

static int count_new_findings(void) {
    /* Compare current scan findings against baseline normal types */
    sqlite3_stmt *stmt;
    char baseline_types[2048] = {0};
    if (sqlite3_prepare_v2(db,
            "SELECT normal_finding_types FROM behavioural_baseline ORDER BY id DESC LIMIT 1;",
            -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *t = (const char*)sqlite3_column_text(stmt, 0);
            if (t) strncpy(baseline_types, t, sizeof(baseline_types)-1);
        }
        sqlite3_finalize(stmt);
    }

    /* Count findings in current scan not in baseline */
    int new_count = 0;
    if (sqlite3_prepare_v2(db,
            "SELECT DISTINCT type FROM detections WHERE scan_id = ?;",
            -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, current_scan_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *t = (const char*)sqlite3_column_text(stmt, 0);
            if (t && !strstr(baseline_types, t)) {
                april_log("WARN", "LEO: NEW anomaly type not in baseline: %s", t);
                new_count++;
            }
        }
        sqlite3_finalize(stmt);
    }
    return new_count;
}

/* ── SAR export ───────────────────────────────────────────────────────────── */
void leo_export_sar(void) {
    const char *sar_path =
        SAR_PATH;
    FILE *f = fopen(sar_path, "w");
    if (!f) { april_log("ERROR", "LEO: Cannot write SAR export"); return; }

    fprintf(f, "{\n");
    fprintf(f, "  \"sar_type\": \"Subject Access Request\",\n");
    fprintf(f, "  \"tool\": \"MiuiserPeruser Superhero Mode\",\n");
    fprintf(f, "  \"exported_at\": %ld,\n", (long)time(NULL));
    fprintf(f, "  \"data_controller\": \"Device owner\",\n");
    fprintf(f, "  \"purpose\": \"Personal device security monitoring\",\n");
    fprintf(f, "  \"legal_basis\": \"Legitimate interest — personal device security\",\n");
    fprintf(f, "  \"retention_policy\": \"90 days rolling, purge on request\",\n\n");

    /* Scan history */
    fprintf(f, "  \"scan_history\": [\n");
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db,
            "SELECT timestamp, depth, finding_count, thermal, battery FROM scan_history ORDER BY timestamp DESC;",
            -1, &stmt, NULL) == SQLITE_OK) {
        int first = 1;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            if (!first) fprintf(f, ",\n");
            fprintf(f, "    {\"timestamp\":%lld,\"depth\":\"%s\","
                    "\"findings\":%d,\"thermal\":%d,\"battery\":%d}",
                    (long long)sqlite3_column_int64(stmt, 0),
                    sqlite3_column_text(stmt, 1) ? (const char*)sqlite3_column_text(stmt, 1) : "",
                    sqlite3_column_int(stmt, 2),
                    sqlite3_column_int(stmt, 3),
                    sqlite3_column_int(stmt, 4));
            first = 0;
        }
        sqlite3_finalize(stmt);
    }
    fprintf(f, "\n  ],\n\n");

    /* Detections */
    fprintf(f, "  \"detections\": [\n");
    if (sqlite3_prepare_v2(db,
            "SELECT timestamp, turtle, type, description, priority FROM detections ORDER BY timestamp DESC;",
            -1, &stmt, NULL) == SQLITE_OK) {
        int first = 1;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            if (!first) fprintf(f, ",\n");
            const char *desc = (const char*)sqlite3_column_text(stmt, 3);
            /* Escape JSON special characters */
            char safe_desc[512] = {0};
            if (desc) {
                int j = 0;
                for (int i = 0; desc[i] && j < 508; i++) {
                    switch (desc[i]) {
                        case '"':  safe_desc[j++] = '\\'; safe_desc[j++] = '"';  break;
                        case '\\': safe_desc[j++] = '\\'; safe_desc[j++] = '\\'; break;
                        case '\n': safe_desc[j++] = '\\'; safe_desc[j++] = 'n';  break;
                        case '\r': safe_desc[j++] = '\\'; safe_desc[j++] = 'r';  break;
                        case '\t': safe_desc[j++] = '\\'; safe_desc[j++] = 't';  break;
                        default:   safe_desc[j++] = desc[i]; break;
                    }
                }
                safe_desc[j] = '\0';
            }
            fprintf(f, "    {\"timestamp\":%lld,\"turtle\":\"%s\","
                    "\"type\":\"%s\",\"description\":\"%s\",\"priority\":\"%s\"}",
                    (long long)sqlite3_column_int64(stmt, 0),
                    sqlite3_column_text(stmt, 1) ? (const char*)sqlite3_column_text(stmt, 1) : "",
                    sqlite3_column_text(stmt, 2) ? (const char*)sqlite3_column_text(stmt, 2) : "",
                    safe_desc,
                    sqlite3_column_text(stmt, 4) ? (const char*)sqlite3_column_text(stmt, 4) : "");
            first = 0;
        }
        sqlite3_finalize(stmt);
    }
    fprintf(f, "\n  ],\n\n");

    fprintf(f, "  \"gdpr_rights\": {\n");
    fprintf(f, "    \"right_to_access\": \"This document\",\n");
    fprintf(f, "    \"right_to_erasure\": \"Run: superhero --purge\",\n");
    fprintf(f, "    \"right_to_portability\": \"JSON format — machine readable\",\n");
    fprintf(f, "    \"data_location\": \"On-device only — never transmitted\"\n");
    fprintf(f, "  }\n}\n");

    fclose(f);
    chmod(sar_path, 0600);
    db_exec("INSERT INTO gdpr_log (action, detail) VALUES "
            "('SAR_EXPORT', 'Subject Access Request export generated');");
    april_log("INFO", "LEO: SAR export written to data/SAR_export.json");
    printf("%s [GDPR] SAR export complete: data/SAR_export.json\n", LOG_PREFIX);
}

/* ── Purge ────────────────────────────────────────────────────────────────── */
void leo_purge_all(void) {
    db_exec("DELETE FROM detections;");
    db_exec("DELETE FROM scan_history;");
    db_exec("DELETE FROM behavioural_baseline;");
    db_exec("INSERT INTO gdpr_log (action, detail) VALUES "
            "('DATA_PURGE', 'All scan data purged on user request — right to erasure exercised');");
    april_log("INFO", "LEO: All data purged. GDPR right to erasure exercised.");
}

/* ── Init ─────────────────────────────────────────────────────────────────── */
static void signal_handler(int sig) { running = 0; }

SENSEI_STATUS leo_init(void) {
    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        printf("%s SQLite open failed\n", LOG_PREFIX);
        return SENSEI_STATUS_ERROR;
    }
    create_schema();

    /* Restore cycle count from persisted scan history */
    {
        sqlite3_stmt *stmt;
        const char *sql = "SELECT COUNT(*) FROM scan_history;";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW)
                cycle = sqlite3_column_int(stmt, 0);
            sqlite3_finalize(stmt);
        }
        if (cycle >= 12) cycle = 12; /* cap — baseline already established */
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    printf("%s Turtles Behaviour Anomaly Scanner initialized — deep rolling live feed with learning active\n", LOG_PREFIX);
    return SENSEI_STATUS_OK;
}

/* ── Full scan surface (called by splinter_dojo before turtle scans) ──────── */
SENSEI_STATUS leo_full_scan(void) {
    cycle++;
    if (verbose_mode)
        april_log("INFO", "LEO: Verbose mode active — all checks will be logged");

    /* Create scan record */
    current_scan_id = write_scan_history("surface", 0, 0, -1, -1, -1);

    printf("%s Starting deep behaviour anomaly scan — Cycle %d...\n\n", LOG_PREFIX, cycle);

    /* Baseline management */
    if (!baseline_exists() && cycle >= BASELINE_CYCLES) {
        establish_baseline(cycle);
    }

    if (cycle < BASELINE_CYCLES) {
        printf("%s [LEARNING] Building device baseline... (Cycle %d/%d)\n",
               LOG_PREFIX, cycle, BASELINE_CYCLES);
        printf("%s [LEARNING] Normal behaviour being recorded — findings in this phase are reference data\n",
               LOG_PREFIX);
    } else {
        printf("%s [LEARNING] Comparing against device baseline...\n", LOG_PREFIX);
    }

    return SENSEI_STATUS_OK;
}

/* ── Post-scan summary (called by splinter_dojo after all turtle scans) ───── */
void leo_post_scan(int finding_count, int thermal, int battery, int cpu_freq,
                   const char *depth) {
    int new_findings = 0;
    if (baseline_exists()) new_findings = count_new_findings();

    /* Update scan record — parameterized to prevent SQL injection */
    {
        const char *upd_sql =
            "UPDATE scan_history SET finding_count=?, new_findings=?, "
            "thermal=?, battery=?, cpu_freq=?, depth=? WHERE id=?;";
        sqlite3_stmt *upd_stmt;
        if (sqlite3_prepare_v2(db, upd_sql, -1, &upd_stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int (upd_stmt, 1, finding_count);
            sqlite3_bind_int (upd_stmt, 2, new_findings);
            sqlite3_bind_int (upd_stmt, 3, thermal);
            sqlite3_bind_int (upd_stmt, 4, battery);
            sqlite3_bind_int (upd_stmt, 5, cpu_freq);
            sqlite3_bind_text(upd_stmt, 6, depth, -1, SQLITE_STATIC);
            sqlite3_bind_int (upd_stmt, 7, current_scan_id);
            sqlite3_step(upd_stmt);
            sqlite3_finalize(upd_stmt);
        }
    }

    if (new_findings > 0)
        april_log("WARN", "LEO: %d new anomaly type(s) not seen in baseline", new_findings);
    else if (baseline_exists())
        april_log("INFO", "LEO: All findings match baseline profile — no new anomalies");

    printf("%s Scan complete — %d finding(s), %d new since baseline\n",
           LOG_PREFIX, finding_count, new_findings);

    db_exec("INSERT INTO gdpr_log (action, detail) VALUES "
            "('SCAN_COMPLETE', 'Scan data stored on-device only');");
}

void leo_shutdown(void) {
    if (db) { sqlite3_close(db); db = NULL; }
    printf("%s Turtles scanner shutdown complete.\n", LOG_PREFIX);
}
