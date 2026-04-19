#include "turtle_telemetry.h"
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

static sqlite3 *db = NULL;

void turtle_telemetry_init(const char *db_path)
{
    if (sqlite3_open(db_path, &db) != SQLITE_OK)
        return;

    FILE *f = fopen("src/core/backend/turtle_schema.sql", "r");
    if (!f) return;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);

    char *sql = malloc(len + 1);
    fread(sql, 1, len, f);
    sql[len] = '\0';

    sqlite3_exec(db, sql, 0, 0, 0);

    free(sql);
    fclose(f);
}

void turtle_telemetry_log(
    const char *event_type,
    const char *cmd,
    backend_type_t expected,
    backend_type_t actual,
    int success,
    const char *metadata
)
{
    if (!db) return;

    const char *sql =
        "INSERT INTO turtle_events "
        "(timestamp, event_type, backend_expected, backend_actual, command, success, metadata) "
        "VALUES (?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    sqlite3_bind_int64(stmt, 1, time(NULL));
    sqlite3_bind_text(stmt, 2, event_type, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, backend_name(expected), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, backend_name(actual), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, cmd ? cmd : "", -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 6, success);
    sqlite3_bind_text(stmt, 7, metadata ? metadata : "", -1, SQLITE_STATIC);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}
