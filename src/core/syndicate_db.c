#include "syndicate_db.h"
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

static sqlite3 *db = NULL;

void syndicate_db_init(const char* path) {
    if (sqlite3_open(path, &db) != SQLITE_OK) {
        fprintf(stderr, "[SYNDICATE] Error: %s\n", sqlite3_errmsg(db));
        return;
    }
    const char *sql = "CREATE TABLE IF NOT EXISTS archives ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                      "turtle TEXT, level TEXT, message TEXT, "
                      "stamp DATETIME DEFAULT CURRENT_TIMESTAMP);";
    sqlite3_exec(db, sql, 0, 0, 0);
}

void syndicate_db_log(const char* turtle, const char* level, const char* message) {
    if (!db) return;
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO archives (turtle, level, message) VALUES (?, ?, ?);";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, turtle, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, level, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, message, -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void syndicate_db_close() {
    if (db) sqlite3_close(db);
}
