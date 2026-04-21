#ifndef SYN_CORE_H
#define SYN_CORE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

#define BASE_DIR     "/data/data/com.termux/files/home/MiuiserPeruser"
#define LOG_PATH_FMT BASE_DIR "/Log_Cabin/%s.log"
#define DB_PATH      BASE_DIR "/Database/syndicate.db"

static inline char *backend_exec(const char *cmd) {
    FILE *f = popen(cmd, "r");
    if (!f) return strdup("N/A");
    char *buf = malloc(1024);
    if (!buf) { pclose(f); return strdup("N/A"); }
    size_t n = fread(buf, 1, 1023, f);
    buf[n] = '\0';
    pclose(f);
    return buf;
}

static inline void log_cabin(const char *daemon, const char *msg) {
    char path[512];
    snprintf(path, sizeof(path), LOG_PATH_FMT, daemon);
    FILE *f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "%s\n", msg);
    fclose(f);
}

static inline void syndicate_init(void) {
    sqlite3 *db;
    if (sqlite3_open(DB_PATH, &db) == SQLITE_OK) {
        sqlite3_exec(db,
            "CREATE TABLE IF NOT EXISTS daemon_events ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "daemon TEXT, type TEXT, message TEXT,"
            "ts DATETIME DEFAULT CURRENT_TIMESTAMP);",
            0, 0, 0);
        sqlite3_close(db);
    }
}

static inline void db_log(const char *daemon, const char *type, const char *msg) {
    sqlite3 *db;
    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) return;
    char sql[1024];
    snprintf(sql, sizeof(sql),
        "INSERT INTO daemon_events (daemon,type,message) VALUES ('%s','%s','%s');",
        daemon, type, msg);
    sqlite3_exec(db, sql, 0, 0, 0);
    sqlite3_close(db);
}

#endif
