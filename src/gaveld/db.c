#define _GNU_SOURCE
#include "db.h"
#include "config.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

static sqlite3         *g_db    = NULL;
static pthread_mutex_t  g_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ── Internal helpers ────────────────────────────────────────────────────── */

#define DB_LOCK()   pthread_mutex_lock(&g_mutex)
#define DB_UNLOCK() pthread_mutex_unlock(&g_mutex)

static int db_exec(const char *sql) {
    char *err = NULL;
    int rc = sqlite3_exec(g_db, sql, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        glog("ERROR", "db_exec failed: %s — %s", sql, err ? err : "?");
        sqlite3_free(err);
    }
    return rc;
}

/* ── Open / close ────────────────────────────────────────────────────────── */

int db_open(void) {
    if (sqlite3_open(DB_PATH, &g_db) != SQLITE_OK) {
        glog("ERROR", "sqlite3_open(%s): %s", DB_PATH, sqlite3_errmsg(g_db));
        return -1;
    }
    db_exec("PRAGMA journal_mode=WAL;");
    db_exec("PRAGMA synchronous=NORMAL;");
    db_exec("PRAGMA foreign_keys=ON;");
    glog("INFO", "database opened: %s", DB_PATH);
    return 0;
}

void db_close(void) {
    if (g_db) {
        sqlite3_close(g_db);
        g_db = NULL;
    }
}

/* ── Schema ──────────────────────────────────────────────────────────────── */

int db_init_schema(void) {
    DB_LOCK();
    int rc = db_exec(
        /* threat_scores */
        "CREATE TABLE IF NOT EXISTS threat_scores ("
        "  source             TEXT PRIMARY KEY,"
        "  score              REAL    DEFAULT 0,"
        "  last_updated       INTEGER DEFAULT 0,"
        "  verdict_state      TEXT    DEFAULT 'CLEAN',"
        "  prior_jails        INTEGER DEFAULT 0,"
        "  prior_quarantines  INTEGER DEFAULT 0"
        ");"

        /* signal_window */
        "CREATE TABLE IF NOT EXISTS signal_window ("
        "  id      INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  source  TEXT    NOT NULL,"
        "  signal  TEXT    NOT NULL,"
        "  epoch   INTEGER NOT NULL"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_sw_source_epoch "
        "  ON signal_window(source, epoch);"

        /* cases */
        "CREATE TABLE IF NOT EXISTS cases ("
        "  case_id   TEXT PRIMARY KEY,"
        "  source    TEXT    NOT NULL,"
        "  signal    TEXT    NOT NULL,"
        "  score     REAL    DEFAULT 0,"
        "  context   TEXT    DEFAULT '',"
        "  status    TEXT    DEFAULT 'PENDING_JUDGEMENT',"
        "  created   INTEGER NOT NULL"
        ");"

        /* verdicts */
        "CREATE TABLE IF NOT EXISTS verdicts ("
        "  id               INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  case_id          TEXT    NOT NULL,"
        "  source           TEXT    NOT NULL,"
        "  verdict          TEXT    NOT NULL,"
        "  score            REAL    DEFAULT 0,"
        "  epoch            INTEGER NOT NULL,"
        "  consent_required INTEGER DEFAULT 0,"
        "  consent_granted  INTEGER DEFAULT 0"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_verd_source "
        "  ON verdicts(source, epoch);"

        /* criminal_record */
        "CREATE TABLE IF NOT EXISTS criminal_record ("
        "  id      INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  source  TEXT    NOT NULL,"
        "  verdict TEXT    NOT NULL,"
        "  reason  TEXT    DEFAULT '',"
        "  actor   TEXT    DEFAULT '',"
        "  epoch   INTEGER NOT NULL"
        ");"

        /* scoring_log */
        "CREATE TABLE IF NOT EXISTS scoring_log ("
        "  id              INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  epoch           INTEGER NOT NULL,"
        "  source          TEXT    NOT NULL,"
        "  signal          TEXT    NOT NULL,"
        "  base_weight     REAL    DEFAULT 0,"
        "  final_addition  REAL    DEFAULT 0,"
        "  prev_score      REAL    DEFAULT 0,"
        "  new_score       REAL    DEFAULT 0,"
        "  verdict_state   TEXT    DEFAULT '',"
        "  modifiers       TEXT    DEFAULT ''"
        ");"

        /* consent_queue */
        "CREATE TABLE IF NOT EXISTS consent_queue ("
        "  case_id         TEXT PRIMARY KEY,"
        "  source          TEXT    NOT NULL,"
        "  verdict         TEXT    NOT NULL,"
        "  score           REAL    DEFAULT 0,"
        "  queued          INTEGER NOT NULL,"
        "  timeout_secs    INTEGER DEFAULT 0,"
        "  timeout_action  TEXT    DEFAULT 'hold'"
        ");"

        /* audit_log */
        "CREATE TABLE IF NOT EXISTS audit_log ("
        "  id      INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  epoch   INTEGER NOT NULL,"
        "  audit_check TEXT    NOT NULL,"
        "  result  TEXT    NOT NULL,"
        "  detail  TEXT    DEFAULT ''"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_audit_check_epoch "
        "  ON audit_log(audit_check, epoch);"
    );
    DB_UNLOCK();
    if (rc == SQLITE_OK)
        glog("INFO", "schema initialised");
    return (rc == SQLITE_OK) ? 0 : -1;
}

/* ── threat_scores ───────────────────────────────────────────────────────── */

int db_threat_load(const char *source, db_threat_t *out) {
    const char *sql =
        "SELECT score,verdict_state,prior_jails,prior_quarantines,last_updated "
        "FROM threat_scores WHERE source=?;";
    sqlite3_stmt *stmt;
    int found = 0;

    /* safe defaults */
    memset(out, 0, sizeof(*out));
    strncpy(out->source, source, sizeof(out->source)-1);
    strncpy(out->state,  "CLEAN", sizeof(out->state)-1);

    DB_LOCK();
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, source, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            out->score             = sqlite3_column_double(stmt, 0);
            const char *st = (const char *)sqlite3_column_text(stmt, 1);
            if (st) strncpy(out->state, st, sizeof(out->state)-1);
            out->prior_jails       = sqlite3_column_int(stmt, 2);
            out->prior_quarantines = sqlite3_column_int(stmt, 3);
            out->last_updated      = (time_t)sqlite3_column_int64(stmt, 4);
            found = 1;
        }
        sqlite3_finalize(stmt);
    }
    DB_UNLOCK();
    return found;
}

int db_threat_upsert(const db_threat_t *rec) {
    const char *sql =
        "INSERT INTO threat_scores"
        "(source,score,last_updated,verdict_state,prior_jails,prior_quarantines)"
        " VALUES(?,?,?,?,?,?)"
        " ON CONFLICT(source) DO UPDATE SET"
        "  score=excluded.score,"
        "  last_updated=excluded.last_updated,"
        "  verdict_state=excluded.verdict_state,"
        "  prior_jails=excluded.prior_jails,"
        "  prior_quarantines=excluded.prior_quarantines;";
    sqlite3_stmt *stmt;
    int rc;

    DB_LOCK();
    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, rec->source, -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 2, rec->score);
        sqlite3_bind_int64(stmt, 3, (sqlite3_int64)rec->last_updated);
        sqlite3_bind_text(stmt, 4, rec->state, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 5, rec->prior_jails);
        sqlite3_bind_int(stmt, 6, rec->prior_quarantines);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    DB_UNLOCK();
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_threat_all(db_threat_t *out, int max, int *count) {
    const char *sql =
        "SELECT source,score,verdict_state,prior_jails,prior_quarantines,last_updated "
        "FROM threat_scores;";
    sqlite3_stmt *stmt;
    *count = 0;

    DB_LOCK();
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        DB_UNLOCK();
        return -1;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW && *count < max) {
        db_threat_t *r = &out[*count];
        const char *src = (const char *)sqlite3_column_text(stmt, 0);
        const char *st  = (const char *)sqlite3_column_text(stmt, 2);
        if (src) strncpy(r->source, src, sizeof(r->source)-1);
        r->score             = sqlite3_column_double(stmt, 1);
        if (st) strncpy(r->state, st, sizeof(r->state)-1);
        r->prior_jails       = sqlite3_column_int(stmt, 3);
        r->prior_quarantines = sqlite3_column_int(stmt, 4);
        r->last_updated      = (time_t)sqlite3_column_int64(stmt, 5);
        (*count)++;
    }
    sqlite3_finalize(stmt);
    DB_UNLOCK();
    return 0;
}

/* ── signal_window ───────────────────────────────────────────────────────── */

int db_signal_insert(const char *source, const char *signal, time_t epoch) {
    const char *sql =
        "INSERT INTO signal_window(source,signal,epoch) VALUES(?,?,?);";
    sqlite3_stmt *stmt;
    int rc;

    DB_LOCK();
    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, source, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, signal, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 3, (sqlite3_int64)epoch);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    DB_UNLOCK();
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_signal_prune(time_t cutoff) {
    char sql[128];
    snprintf(sql, sizeof(sql),
        "DELETE FROM signal_window WHERE epoch < %ld;", (long)cutoff);
    DB_LOCK();
    db_exec(sql);
    DB_UNLOCK();
    return 0;
}

int db_signal_distinct(const char *source, time_t since,
                        char signals[16][64], int *count) {
    const char *sql =
        "SELECT DISTINCT signal FROM signal_window "
        "WHERE source=? AND epoch>? LIMIT 16;";
    sqlite3_stmt *stmt;
    *count = 0;

    DB_LOCK();
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, source, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, (sqlite3_int64)since);
        while (sqlite3_step(stmt) == SQLITE_ROW && *count < 16) {
            const char *sig = (const char *)sqlite3_column_text(stmt, 0);
            if (sig) strncpy(signals[(*count)++], sig, 63);
        }
        sqlite3_finalize(stmt);
    }
    DB_UNLOCK();
    return 0;
}

int db_signal_active(const char *source, time_t since) {
    const char *sql =
        "SELECT 1 FROM signal_window WHERE source=? AND epoch>? LIMIT 1;";
    sqlite3_stmt *stmt;
    int active = 0;

    DB_LOCK();
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, source, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, (sqlite3_int64)since);
        active = (sqlite3_step(stmt) == SQLITE_ROW);
        sqlite3_finalize(stmt);
    }
    DB_UNLOCK();
    return active;
}

/* ── cases ───────────────────────────────────────────────────────────────── */

int db_case_insert(const db_case_t *c) {
    const char *sql =
        "INSERT OR IGNORE INTO cases"
        "(case_id,source,signal,score,context,status,created)"
        " VALUES(?,?,?,?,?,?,?);";
    sqlite3_stmt *stmt;
    int rc;

    DB_LOCK();
    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, c->case_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, c->source,  -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, c->signal,  -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 4, c->score);
        sqlite3_bind_text(stmt, 5, c->context, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 6, c->status,  -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 7, (sqlite3_int64)c->created);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    DB_UNLOCK();
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_case_update_status(const char *case_id, const char *status) {
    const char *sql =
        "UPDATE cases SET status=? WHERE case_id=?;";
    sqlite3_stmt *stmt;
    int rc;

    DB_LOCK();
    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, status,  -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, case_id, -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    DB_UNLOCK();
    return (rc == SQLITE_DONE) ? 0 : -1;
}

/* ── verdicts ────────────────────────────────────────────────────────────── */

int db_verdict_insert(const db_verdict_t *v) {
    const char *sql =
        "INSERT INTO verdicts"
        "(case_id,source,verdict,score,epoch,consent_required,consent_granted)"
        " VALUES(?,?,?,?,?,?,?);";
    sqlite3_stmt *stmt;
    int rc;

    DB_LOCK();
    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, v->case_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, v->source,  -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, v->verdict, -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 4, v->score);
        sqlite3_bind_int64(stmt, 5, (sqlite3_int64)v->epoch);
        sqlite3_bind_int(stmt, 6, v->consent_required);
        sqlite3_bind_int(stmt, 7, v->consent_granted);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    DB_UNLOCK();
    return (rc == SQLITE_DONE) ? 0 : -1;
}


/* Total verdicts since epoch — no source/verdict filter */
int db_verdict_count_all(time_t since) {
    const char *sql = "SELECT COUNT(*) FROM verdicts WHERE epoch>?;";
    sqlite3_stmt *stmt;
    int count = 0;
    DB_LOCK();
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, (sqlite3_int64)since);
        if (sqlite3_step(stmt) == SQLITE_ROW)
            count = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }
    DB_UNLOCK();
    return count;
}

/* Verdicts matching a specific verdict type, any source */
int db_verdict_count_by_type(const char *verdict, time_t since) {
    const char *sql = "SELECT COUNT(*) FROM verdicts WHERE verdict=? AND epoch>?;";
    sqlite3_stmt *stmt;
    int count = 0;
    DB_LOCK();
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, verdict, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, (sqlite3_int64)since);
        if (sqlite3_step(stmt) == SQLITE_ROW)
            count = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }
    DB_UNLOCK();
    return count;
}

int db_verdict_count_recent(const char *source, const char *verdict,
                             time_t since) {
    const char *sql =
        "SELECT COUNT(*) FROM verdicts "
        "WHERE source=? AND verdict=? AND epoch>?;";
    sqlite3_stmt *stmt;
    int count = 0;

    DB_LOCK();
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, source,  -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, verdict, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 3, (sqlite3_int64)since);
        if (sqlite3_step(stmt) == SQLITE_ROW)
            count = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }
    DB_UNLOCK();
    return count;
}

/* ── criminal_record ─────────────────────────────────────────────────────── */

int db_criminal_record_insert(const char *source, const char *verdict,
                               const char *reason, const char *actor,
                               time_t epoch) {
    const char *sql =
        "INSERT INTO criminal_record(source,verdict,reason,actor,epoch)"
        " VALUES(?,?,?,?,?);";
    sqlite3_stmt *stmt;
    int rc;

    DB_LOCK();
    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, source,  -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, verdict, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, reason,  -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, actor,   -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 5, (sqlite3_int64)epoch);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    DB_UNLOCK();
    return (rc == SQLITE_DONE) ? 0 : -1;
}

/* ── scoring_log ─────────────────────────────────────────────────────────── */

int db_scoring_log_insert(time_t epoch, const char *source,
                           const char *signal, double base_weight,
                           double final_addition, double prev_score,
                           double new_score, const char *state,
                           const char *modifiers) {
    const char *sql =
        "INSERT INTO scoring_log"
        "(epoch,source,signal,base_weight,final_addition,"
        " prev_score,new_score,verdict_state,modifiers)"
        " VALUES(?,?,?,?,?,?,?,?,?);";
    sqlite3_stmt *stmt;
    int rc;

    DB_LOCK();
    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, (sqlite3_int64)epoch);
        sqlite3_bind_text(stmt, 2, source,    -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, signal,    -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 4, base_weight);
        sqlite3_bind_double(stmt, 5, final_addition);
        sqlite3_bind_double(stmt, 6, prev_score);
        sqlite3_bind_double(stmt, 7, new_score);
        sqlite3_bind_text(stmt, 8, state,     -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 9, modifiers, -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    DB_UNLOCK();
    return (rc == SQLITE_DONE) ? 0 : -1;
}

/* ── consent_queue ───────────────────────────────────────────────────────── */

int db_consent_insert(const db_consent_t *c) {
    const char *sql =
        "INSERT OR REPLACE INTO consent_queue"
        "(case_id,source,verdict,score,queued,timeout_secs,timeout_action)"
        " VALUES(?,?,?,?,?,?,?);";
    sqlite3_stmt *stmt;
    int rc;

    DB_LOCK();
    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, c->case_id,        -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, c->source,         -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, c->verdict,        -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 4, c->score);
        sqlite3_bind_int64(stmt, 5, (sqlite3_int64)c->queued);
        sqlite3_bind_int(stmt, 6, c->timeout_secs);
        sqlite3_bind_text(stmt, 7, c->timeout_action, -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    DB_UNLOCK();
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_consent_remove(const char *case_id) {
    const char *sql = "DELETE FROM consent_queue WHERE case_id=?;";
    sqlite3_stmt *stmt;
    int rc;

    DB_LOCK();
    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, case_id, -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    DB_UNLOCK();
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_consent_pending(db_consent_t *out, int max, int *count) {
    const char *sql =
        "SELECT case_id,source,verdict,score,queued,timeout_secs,timeout_action "
        "FROM consent_queue ORDER BY queued ASC;";
    sqlite3_stmt *stmt;
    *count = 0;

    DB_LOCK();
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW && *count < max) {
            db_consent_t *r = &out[*count];
            const char *cid = (const char *)sqlite3_column_text(stmt, 0);
            const char *src = (const char *)sqlite3_column_text(stmt, 1);
            const char *vrd = (const char *)sqlite3_column_text(stmt, 2);
            const char *act = (const char *)sqlite3_column_text(stmt, 6);
            if (cid) strncpy(r->case_id,        cid, sizeof(r->case_id)-1);
            if (src) strncpy(r->source,          src, sizeof(r->source)-1);
            if (vrd) strncpy(r->verdict,         vrd, sizeof(r->verdict)-1);
            if (act) strncpy(r->timeout_action,  act, sizeof(r->timeout_action)-1);
            r->score        = sqlite3_column_double(stmt, 3);
            r->queued       = (time_t)sqlite3_column_int64(stmt, 4);
            r->timeout_secs = sqlite3_column_int(stmt, 5);
            (*count)++;
        }
        sqlite3_finalize(stmt);
    }
    DB_UNLOCK();
    return 0;
}

/* ── audit_log ───────────────────────────────────────────────────────────── */

int db_audit_log_insert(time_t epoch, const char *check,
                         const char *result, const char *detail) {
    const char *sql =
        "INSERT INTO audit_log(epoch,audit_check,result,detail) VALUES(?,?,?,?);";
    sqlite3_stmt *stmt;
    int rc;

    DB_LOCK();
    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, (sqlite3_int64)epoch);
        sqlite3_bind_text(stmt, 2, check,  -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, result, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, detail, -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    DB_UNLOCK();
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_audit_flag_count(const char *check, time_t since) {
    const char *sql =
        "SELECT COUNT(*) FROM audit_log "
        "WHERE audit_check=? AND result='FLAG' AND epoch>?;";
    sqlite3_stmt *stmt;
    int count = 0;

    DB_LOCK();
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, check, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, (sqlite3_int64)since);
        if (sqlite3_step(stmt) == SQLITE_ROW)
            count = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }
    DB_UNLOCK();
    return count;
}

/* ── WAL checkpoint ──────────────────────────────────────────────────────── */

void db_checkpoint(void) {
    DB_LOCK();
    sqlite3_exec(g_db, "PRAGMA wal_checkpoint(TRUNCATE);", NULL, NULL, NULL);
    DB_UNLOCK();
}
