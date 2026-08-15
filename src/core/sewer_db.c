/*
 * sewer_db.c — accumulation store + rollup/notification trigger for splinterd.
 *
 * Design:
 *   - emit_state: one row per (source, event_type), upserted every emission.
 *     Repeats bump repeat_count/last_seen instead of growing the table.
 *   - emit_transitions: append-only, but only written on a real change:
 *     first time a (source, event_type) pair is seen, or it resumes after
 *     a silence gap. This is the "interesting" filter — noise never reaches
 *     this table by construction.
 *   - When emit_transitions crosses ROLLUP_CAP rows: summarize, write to
 *     sewer_rats, fire an on-device notification via `cmd notification post`
 *     (through rish — no termux-api dependency), then trim the table.
 *
 * Only splinterd opens this db.
 */

#include "sewer_db.h"
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#define ROLLUP_CAP          500   /* rows in emit_transitions before we roll up + notify */
#define GAP_THRESHOLD_SEC   1800  /* 30 min silence before a re-appearance counts as "resumed" */
#define RISH_PATH           "/data/data/com.termux/files/home/rish"

static sqlite3 *g_db = NULL;

static void now_iso(char *buf, size_t buflen)
{
    time_t t = time(NULL);
    strftime(buf, buflen, "%Y-%m-%dT%H:%M:%S", localtime(&t));
}

int sewer_db_init(const char *path)
{
    if (sqlite3_open(path, &g_db) != SQLITE_OK) {
        g_db = NULL;
        return -1;
    }
    sqlite3_exec(g_db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
    sqlite3_exec(g_db, "PRAGMA auto_vacuum=INCREMENTAL;", NULL, NULL, NULL);
    return 0;
}

void sewer_db_close(void)
{
    if (g_db) { sqlite3_close(g_db); g_db = NULL; }
}

/* fork+execl, not system()/popen() — same reasoning as run_via_pty() elsewhere
 * in the fleet: avoid shell interpolation of daemon-sourced strings. */
/* Single-quote a string for safe embedding in a shell command line.
 * Escapes embedded single quotes via the standard '\'' technique.
 * summary is our own generated text, but it's built from wire-sourced
 * daemon/event-type names, so treat it as untrusted for quoting purposes. */
static void shell_quote(const char *in, char *out, size_t outsize)
{
    size_t o = 0;
    if (o < outsize - 1) out[o++] = '\'';
    for (const char *p = in; *p && o < outsize - 5; p++) {
        if (*p == '\'') {
            out[o++] = '\''; out[o++] = '\\'; out[o++] = '\''; out[o++] = '\'';
        } else {
            out[o++] = *p;
        }
    }
    if (o < outsize - 1) out[o++] = '\'';
    out[o] = '\0';
}

static void fire_notification(const char *summary)
{
    pid_t pid = fork();
    if (pid < 0) return;
    if (pid == 0) {
        char quoted[1536];
        shell_quote(summary, quoted, sizeof(quoted));

        char cmdbuf[2048];
        snprintf(cmdbuf, sizeof(cmdbuf),
                 "cmd notification post -S bigtext -t 'sewer.db rollup' sewer_rollup %s",
                 quoted);
        execl(RISH_PATH, "rish", "-c", cmdbuf, (char *)NULL);
        /* execl only returns on failure; log it so this isn't silent again */
        fprintf(stderr, "[SEWER_DB/ERROR] execl(%s) failed: %s\n", RISH_PATH, strerror(errno));
        _exit(127);
    }
    /* parent: no waitpid() — don't block splinterd's accept loop on this */
}

static void do_rollup(void)
{
    char ts[32];
    now_iso(ts, sizeof(ts));

    char summary[1024] = {0};
    int off = 0;

    sqlite3_stmt *stmt;
    const char *q =
        "SELECT transition_type, COUNT(*), GROUP_CONCAT(DISTINCT source) "
        "FROM emit_transitions GROUP BY transition_type;";
    if (sqlite3_prepare_v2(g_db, q, -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW && off < (int)sizeof(summary) - 150) {
            const char *ttype   = (const char *)sqlite3_column_text(stmt, 0);
            int cnt              = sqlite3_column_int(stmt, 1);
            const char *sources = (const char *)sqlite3_column_text(stmt, 2);
            off += snprintf(summary + off, sizeof(summary) - off,
                             "%s x%d (%s); ",
                             ttype ? ttype : "?", cnt, sources ? sources : "");
        }
        sqlite3_finalize(stmt);
    }
    if (off == 0) snprintf(summary, sizeof(summary), "no transitions this period");

    sqlite3_stmt *ins;
    const char *insq = "INSERT INTO sewer_rats (rollup_ts, summary) VALUES (?, ?);";
    if (sqlite3_prepare_v2(g_db, insq, -1, &ins, NULL) == SQLITE_OK) {
        sqlite3_bind_text(ins, 1, ts, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(ins, 2, summary, -1, SQLITE_TRANSIENT);
        sqlite3_step(ins);
        sqlite3_finalize(ins);
    }

    fire_notification(summary);

    sqlite3_exec(g_db, "DELETE FROM emit_transitions;", NULL, NULL, NULL);
    sqlite3_exec(g_db, "PRAGMA incremental_vacuum;", NULL, NULL, NULL);
}

static void maybe_rollup(void)
{
    sqlite3_stmt *stmt;
    int count = 0;
    if (sqlite3_prepare_v2(g_db, "SELECT COUNT(*) FROM emit_transitions;", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) count = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }
    if (count >= ROLLUP_CAP) do_rollup();
}

void sewer_db_record_emit(const char *source, const char *event_type, const char *detail)
{
    if (!g_db) return;

    char ts[32];
    now_iso(ts, sizeof(ts));

    /* look up existing state for this (source, event_type) pair */
    sqlite3_stmt *sel;
    char last_seen[32] = {0};
    int found = 0;
    const char *selq = "SELECT last_seen FROM emit_state WHERE source=? AND event_type=?;";
    if (sqlite3_prepare_v2(g_db, selq, -1, &sel, NULL) == SQLITE_OK) {
        sqlite3_bind_text(sel, 1, source, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(sel, 2, event_type, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(sel) == SQLITE_ROW) {
            found = 1;
            const char *ls = (const char *)sqlite3_column_text(sel, 0);
            if (ls) strncpy(last_seen, ls, sizeof(last_seen) - 1);
        }
        sqlite3_finalize(sel);
    }

    const char *transition_type = NULL;
    if (!found) {
        transition_type = "first_seen";
    } else if (last_seen[0]) {
        struct tm tm_last = {0}, tm_now = {0};
        strptime(last_seen, "%Y-%m-%dT%H:%M:%S", &tm_last);
        strptime(ts,        "%Y-%m-%dT%H:%M:%S", &tm_now);
        time_t t_last = mktime(&tm_last);
        time_t t_now  = mktime(&tm_now);
        if (t_now - t_last >= GAP_THRESHOLD_SEC) transition_type = "resumed";
    }

    /* upsert emit_state */
    const char *upq =
        "INSERT INTO emit_state (source, event_type, first_seen, last_seen, repeat_count, last_detail) "
        "VALUES (?, ?, ?, ?, 1, ?) "
        "ON CONFLICT(source, event_type) DO UPDATE SET "
        "last_seen=excluded.last_seen, repeat_count=repeat_count+1, last_detail=excluded.last_detail;";
    sqlite3_stmt *up;
    if (sqlite3_prepare_v2(g_db, upq, -1, &up, NULL) == SQLITE_OK) {
        sqlite3_bind_text(up, 1, source, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(up, 2, event_type, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(up, 3, ts, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(up, 4, ts, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(up, 5, detail ? detail : "", -1, SQLITE_TRANSIENT);
        sqlite3_step(up);
        sqlite3_finalize(up);
    }

    /* only append a transition row when something actually changed */
    if (transition_type) {
        const char *tq =
            "INSERT INTO emit_transitions (source, event_type, transition_type, ts, detail) "
            "VALUES (?, ?, ?, ?, ?);";
        sqlite3_stmt *tstmt;
        if (sqlite3_prepare_v2(g_db, tq, -1, &tstmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(tstmt, 1, source, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(tstmt, 2, event_type, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(tstmt, 3, transition_type, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(tstmt, 4, ts, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(tstmt, 5, detail ? detail : "", -1, SQLITE_TRANSIENT);
            sqlite3_step(tstmt);
            sqlite3_finalize(tstmt);
        }
    }

    /* lifetime totals, unaffected by dedup */
    const char *stq =
        "INSERT INTO daemon_stats (name, total_emitted, last_event_type, last_event_time) "
        "VALUES (?, 1, ?, ?) "
        "ON CONFLICT(name) DO UPDATE SET "
        "total_emitted=total_emitted+1, last_event_type=excluded.last_event_type, "
        "last_event_time=excluded.last_event_time;";
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(g_db, stq, -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, source, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, event_type, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 3, ts, -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
        sqlite3_finalize(st);
    }

    if (transition_type) maybe_rollup();
}
