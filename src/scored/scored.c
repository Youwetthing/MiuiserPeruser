/*
 * scored.c — Judicial Scoring Daemon
 * Part of MiuiserPeruser Judicial System v2
 *
 * Replaces: scoring_engine.sh, parole_engine.sh, signal_window.state,
 *           threat_scores.state
 *
 * Wire protocol (Unix socket: pipes/scored.sock):
 *   IN:  SOURCE|SIGNAL|BASE_WEIGHT|CONTEXT\n
 *   OUT: SOURCE|NEW_SCORE|VERDICT_STATE|SCORE_DELTA\n
 *
 * Special queries:
 *   IN:  QUERY|STATUS\n
 *   OUT: source|score|state|prior_jails|prior_quarantines\n  (one per source)
 *        END\n
 *
 *   IN:  QUERY|DECAY\n
 *   OUT: OK\n  (triggers immediate decay tick)
 *
 * Build:
 *   gcc -o bin/scored src/scored/scored.c -lsqlite3 -lpthread -Wall -O2
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <errno.h>
#include <sqlite3.h>

/* ── Config ─────────────────────────────────────────────────────────────── */

#define SOCKET_NAME     "pipes/scored.sock"
#define DB_NAME         "state/scored.db"
#define LOG_FILE        "logs/scored.log"
#define PID_FILE        "pipes/pids/scored.pid"
#define IA_LOCK         "state/internal_affairs.lock"
#define SOVEREIGNTY     "state/sovereignty.list"

#define DECAY_INTERVAL  120          /* seconds between decay ticks */
#define DECAY_FACTOR    0.88
#define SIGNAL_WINDOW   120          /* seconds for covariance window */
#define MAX_SCORE       100.0
#define SCORE_FLOOR     0.0
#define BACKLOG         32
#define BUF_SIZE        1024

/* ── Score thresholds ────────────────────────────────────────────────────── */

#define THRESH_JAILED       80.0
#define THRESH_HOUSE_ARREST 70.0
#define THRESH_QUARANTINED  50.0
#define THRESH_WARNED       40.0
#define THRESH_WATCHED      20.0

/* ── Globals ─────────────────────────────────────────────────────────────── */

static sqlite3         *db       = NULL;
static pthread_mutex_t  db_mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile int     running  = 1;
static FILE            *logfp    = NULL;
static char             base_dir[512] = {0};

/* ── Logging ─────────────────────────────────────────────────────────────── */

static void slog(const char *level, const char *fmt, ...) {
    if (!logfp) return;
    time_t now = time(NULL);
    va_list ap;
    fprintf(logfp, "[SCORED] %ld [%s] ", (long)now, level);
    va_start(ap, fmt);
    vfprintf(logfp, fmt, ap);
    va_end(ap);
    fprintf(logfp, "\n");
    fflush(logfp);
}

/* ── Score → state string ────────────────────────────────────────────────── */

static const char *score_to_state(double score) {
    if (score >= THRESH_JAILED)       return "JAILED";
    if (score >= THRESH_HOUSE_ARREST) return "HOUSE_ARREST";
    if (score >= THRESH_QUARANTINED)  return "QUARANTINED";
    if (score >= THRESH_WARNED)       return "WARNED";
    if (score >= THRESH_WATCHED)      return "WATCHED";
    return "CLEAN";
}

/* ── State rank (for de-escalation detection) ───────────────────────────── */

static int state_rank(const char *state) {
    if (!state) return 0;
    if (strcmp(state, "JAILED")       == 0) return 5;
    if (strcmp(state, "HOUSE_ARREST") == 0) return 4;
    if (strcmp(state, "QUARANTINED")  == 0) return 3;
    if (strcmp(state, "WARNED")       == 0) return 2;
    if (strcmp(state, "WATCHED")      == 0) return 1;
    return 0;
}

/* ── DB init ─────────────────────────────────────────────────────────────── */

static int db_init(void) {
    char db_path[512];
    snprintf(db_path, sizeof(db_path), "%s/%s", base_dir, DB_NAME);

    if (sqlite3_open(db_path, &db) != SQLITE_OK) {
        slog("ERROR", "sqlite3_open failed: %s", sqlite3_errmsg(db));
        return -1;
    }

    /* WAL mode for concurrent reads */
    sqlite3_exec(db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
    sqlite3_exec(db, "PRAGMA synchronous=NORMAL;", NULL, NULL, NULL);

    const char *schema =
        "CREATE TABLE IF NOT EXISTS threat_scores ("
        "  source           TEXT PRIMARY KEY,"
        "  score            REAL DEFAULT 0,"
        "  last_updated     INTEGER,"
        "  verdict_state    TEXT DEFAULT 'CLEAN',"
        "  prior_jails      INTEGER DEFAULT 0,"
        "  prior_quarantines INTEGER DEFAULT 0"
        ");"
        "CREATE TABLE IF NOT EXISTS signal_window ("
        "  id      INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  source  TEXT,"
        "  signal  TEXT,"
        "  epoch   INTEGER"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_sw ON signal_window(source, epoch);"
        "CREATE TABLE IF NOT EXISTS scoring_log ("
        "  id             INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  epoch          INTEGER,"
        "  source         TEXT,"
        "  signal         TEXT,"
        "  base_weight    REAL,"
        "  final_addition REAL,"
        "  prev_score     REAL,"
        "  new_score      REAL,"
        "  verdict_state  TEXT,"
        "  modifiers      TEXT"
        ");";

    char *err = NULL;
    if (sqlite3_exec(db, schema, NULL, NULL, &err) != SQLITE_OK) {
        slog("ERROR", "schema init failed: %s", err);
        sqlite3_free(err);
        return -1;
    }

    slog("INFO", "database initialised at %s", db_path);
    return 0;
}

/* ── Tier modifier ───────────────────────────────────────────────────────── */

static double get_tier_modifier(const char *source) {
    /* MiuiserPeruser own daemons */
    const char *own_daemons[] = {
        "splinterd","krangd","rahzerd","leatherheadd","metalheadd",
        "metalhead","granitord","ratkingd","shredderd","fugitoidd",
        "bebopd","burned","rocksteadyd","tigerclawd","turtlecomd",
        "connectivityd","networkd","foot_clan_supreme","foot_portwatchd",
        "foot_resurrectord","foot_ipcshadowd","footrunner","cpud",
        "processd","storaged","thermald","sysportd","daemonhunterd",
        "miuiserperuser","miuiserperuser-daemon","miuid","brain-ctl",
        "april_o_neil","court_orchestrator","court_core_engine",
        "judge_executor","parole_engine","scoring_engine","scored",
        "internal_affairs","consent_gate","escalation_daemon",
        "visitors_pass_daemon","turtlepower_daemon","superhero_adapter",
        "baxter_stockman","court_dispatcher",
        NULL
    };

    /* Check basename */
    const char *base = strrchr(source, '/');
    base = base ? base + 1 : source;

    for (int i = 0; own_daemons[i]; i++) {
        if (strcmp(base, own_daemons[i]) == 0) return 0.40;
    }

    /* Sovereignty list */
    char sov_path[512];
    snprintf(sov_path, sizeof(sov_path), "%s/%s", base_dir, SOVEREIGNTY);
    FILE *fp = fopen(sov_path, "r");
    if (fp) {
        char line[512];
        while (fgets(line, sizeof(line), fp)) {
            if (line[0] == '#') continue;
            char pkg[256];
            if (sscanf(line, "%255[^|]", pkg) == 1) {
                if (strcmp(pkg, source) == 0) {
                    fclose(fp);
                    return 0.15;
                }
            }
        }
        fclose(fp);
    }

    /* MIUI / AOSP prefixes */
    const char *miui_prefixes[] = {
        "com.miui.","com.xiaomi.","com.hyperos.","android.",
        "com.android.","miui.","com.lbe.","com.qualcomm.",
        "com.qti.","org.codeaurora.",
        NULL
    };
    for (int i = 0; miui_prefixes[i]; i++) {
        if (strncmp(source, miui_prefixes[i], strlen(miui_prefixes[i])) == 0)
            return 0.60;
    }

    return 1.00;
}

/* ── Covariance matrix ───────────────────────────────────────────────────── */

typedef struct {
    char   signals[16][64];  /* up to 16 distinct signals in window */
    int    count;
} SignalSet;

static int sigset_has(const SignalSet *ss, const char *sig) {
    for (int i = 0; i < ss->count; i++)
        if (strcmp(ss->signals[i], sig) == 0) return 1;
    return 0;
}

static double get_covariance(const SignalSet *ss, double *stack_bonus) {
    *stack_bonus = (ss->count >= 3) ? 1.20 : 1.00;

    if (sigset_has(ss, "INTEGRITY_VIOLATION"))                          return 1.80;
    if (sigset_has(ss, "NETWORK_ANOMALY") &&
        (sigset_has(ss, "CPU_HOG") || sigset_has(ss, "CPU_HOG_CRITICAL")))
                                                                        return 1.60;
    if (sigset_has(ss, "WAKELOCK_ANOMALY") &&
        sigset_has(ss, "NETWORK_ANOMALY"))                              return 1.50;
    if (sigset_has(ss, "THERMAL_CRITICAL") &&
        sigset_has(ss, "CPU_HOG_CRITICAL"))                             return 1.40;
    if ((sigset_has(ss, "THERMAL_CRITICAL") ||
         sigset_has(ss, "THERMAL_WARN")) &&
        sigset_has(ss, "NETWORK_ANOMALY"))                              return 1.30;
    if (sigset_has(ss, "CPU_THROTTLING") &&
        sigset_has(ss, "WAKELOCK_ANOMALY"))                             return 1.20;
    return 1.00;
}

/* ── Load signal window for source ──────────────────────────────────────── */

static void load_signal_window(const char *source, SignalSet *ss) {
    ss->count = 0;
    time_t cutoff = time(NULL) - SIGNAL_WINDOW;

    const char *sql =
        "SELECT DISTINCT signal FROM signal_window "
        "WHERE source=? AND epoch>? LIMIT 16;";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return;
    sqlite3_bind_text(stmt, 1, source, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64)cutoff);

    while (sqlite3_step(stmt) == SQLITE_ROW && ss->count < 16) {
        const char *sig = (const char *)sqlite3_column_text(stmt, 0);
        if (sig) strncpy(ss->signals[ss->count++], sig, 63);
    }
    sqlite3_finalize(stmt);
}

/* ── Insert signal into window ───────────────────────────────────────────── */

static void insert_signal_window(const char *source, const char *signal) {
    const char *sql =
        "INSERT INTO signal_window(source,signal,epoch) VALUES(?,?,?);";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return;
    sqlite3_bind_text(stmt, 1, source, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, signal, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, (sqlite3_int64)time(NULL));
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    /* Prune old entries */
    time_t cutoff = time(NULL) - SIGNAL_WINDOW;
    char prune[128];
    snprintf(prune, sizeof(prune),
        "DELETE FROM signal_window WHERE epoch < %ld;", (long)cutoff);
    sqlite3_exec(db, prune, NULL, NULL, NULL);
}

/* ── Load / upsert threat score ──────────────────────────────────────────── */

typedef struct {
    double  score;
    char    state[32];
    int     prior_jails;
    int     prior_quarantines;
    time_t  last_updated;
} ThreatRecord;

static void load_threat(const char *source, ThreatRecord *rec) {
    rec->score = 0.0;
    strncpy(rec->state, "CLEAN", sizeof(rec->state));
    rec->prior_jails = 0;
    rec->prior_quarantines = 0;
    rec->last_updated = 0;

    const char *sql =
        "SELECT score,verdict_state,prior_jails,prior_quarantines,last_updated "
        "FROM threat_scores WHERE source=?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return;
    sqlite3_bind_text(stmt, 1, source, -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        rec->score              = sqlite3_column_double(stmt, 0);
        const char *st          = (const char *)sqlite3_column_text(stmt, 1);
        if (st) strncpy(rec->state, st, sizeof(rec->state)-1);
        rec->prior_jails        = sqlite3_column_int(stmt, 2);
        rec->prior_quarantines  = sqlite3_column_int(stmt, 3);
        rec->last_updated       = (time_t)sqlite3_column_int64(stmt, 4);
    }
    sqlite3_finalize(stmt);
}

static void upsert_threat(const char *source, const ThreatRecord *rec) {
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
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return;
    sqlite3_bind_text(stmt, 1, source, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 2, rec->score);
    sqlite3_bind_int64(stmt, 3, (sqlite3_int64)rec->last_updated);
    sqlite3_bind_text(stmt, 4, rec->state, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, rec->prior_jails);
    sqlite3_bind_int(stmt, 6, rec->prior_quarantines);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

/* ── Core scoring ────────────────────────────────────────────────────────── */

static void score_signal(const char *source, const char *signal,
                          double base_weight,
                          char *out_buf, size_t out_len) {
    time_t now = time(NULL);

    pthread_mutex_lock(&db_mutex);

    /* Load current record */
    ThreatRecord rec;
    load_threat(source, &rec);
    double prev_score = rec.score;

    /* Insert signal into window BEFORE loading covariance
       so current signal participates in its own window */
    insert_signal_window(source, signal);

    /* Load window (now includes current signal) */
    SignalSet ss;
    load_signal_window(source, &ss);

    /* Modifiers */
    double tier_mod      = get_tier_modifier(source);
    double stack_bonus   = 1.00;
    double covariance    = get_covariance(&ss, &stack_bonus);
    double recidivism    = 1.00;
    double user_ctx      = 1.00;    /* TODO: query superhero for fg state */
    double situational   = 1.00;    /* TODO: query turtlecomd thermal state */

    /* Recidivism */
    if (rec.prior_jails >= 2)       recidivism = 2.00;
    else if (rec.prior_jails >= 1)  recidivism = 1.60;
    else if (rec.prior_quarantines >= 1) recidivism = 1.30;

    double addition = base_weight * user_ctx * situational *
                      covariance  * stack_bonus * recidivism * tier_mod;

    double new_score = prev_score + addition;
    if (new_score > MAX_SCORE) new_score = MAX_SCORE;
    if (new_score < SCORE_FLOOR) new_score = SCORE_FLOOR;

    const char *new_state = score_to_state(new_score);
    double delta = new_score - prev_score;

    /* Update record */
    rec.score        = new_score;
    rec.last_updated = now;
    strncpy(rec.state, new_state, sizeof(rec.state)-1);

    /* Update prior counts if newly jailed/quarantined */
    if (strcmp(new_state, "JAILED") == 0 &&
        strcmp(score_to_state(prev_score), "JAILED") != 0)
        rec.prior_jails++;
    if (strcmp(new_state, "QUARANTINED") == 0 &&
        strcmp(score_to_state(prev_score), "QUARANTINED") != 0)
        rec.prior_quarantines++;

    upsert_threat(source, &rec);

    /* Log to scoring_log */
    char modifiers[256];
    snprintf(modifiers, sizeof(modifiers),
        "{\"tier\":%.2f,\"cov\":%.2f,\"stack\":%.2f,"
        "\"recid\":%.2f,\"uctx\":%.2f,\"sit\":%.2f}",
        tier_mod, covariance, stack_bonus, recidivism, user_ctx, situational);

    const char *log_sql =
        "INSERT INTO scoring_log"
        "(epoch,source,signal,base_weight,final_addition,"
        " prev_score,new_score,verdict_state,modifiers)"
        " VALUES(?,?,?,?,?,?,?,?,?);";
    sqlite3_stmt *lstmt;
    if (sqlite3_prepare_v2(db, log_sql, -1, &lstmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(lstmt, 1, (sqlite3_int64)now);
        sqlite3_bind_text(lstmt, 2, source, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(lstmt, 3, signal, -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(lstmt, 4, base_weight);
        sqlite3_bind_double(lstmt, 5, addition);
        sqlite3_bind_double(lstmt, 6, prev_score);
        sqlite3_bind_double(lstmt, 7, new_score);
        sqlite3_bind_text(lstmt, 8, new_state, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(lstmt, 9, modifiers, -1, SQLITE_TRANSIENT);
        sqlite3_step(lstmt);
        sqlite3_finalize(lstmt);
    }

    pthread_mutex_unlock(&db_mutex);

    slog("INFO", "src=%s sig=%s base=%.0f add=%.2f prev=%.2f new=%.2f state=%s",
         source, signal, base_weight, addition, prev_score, new_score, new_state);

    snprintf(out_buf, out_len, "%s|%.2f|%s|%.2f\n",
             source, new_score, new_state, delta);
}

/* ── Decay tick ──────────────────────────────────────────────────────────── */

static void decay_tick(void) {
    /* Check IA lock */
    char ia_path[512];
    snprintf(ia_path, sizeof(ia_path), "%s/%s", base_dir, IA_LOCK);
    if (access(ia_path, F_OK) == 0) {
        slog("INFO", "DECAY_SKIP — internal_affairs.lock active");
        return;
    }

    pthread_mutex_lock(&db_mutex);

    time_t now = time(NULL);
    time_t active_cutoff = now - SIGNAL_WINDOW;

    /* Get all sources */
    const char *sel = "SELECT source,score,verdict_state,prior_jails,prior_quarantines "
                      "FROM threat_scores;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sel, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&db_mutex);
        return;
    }

    typedef struct { char src[256]; double score; char state[32];
                     int pj; int pq; } Row;
    Row rows[512]; int nrows = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW && nrows < 512) {
        const char *src = (const char *)sqlite3_column_text(stmt, 0);
        const char *st  = (const char *)sqlite3_column_text(stmt, 2);
        if (!src || !st) continue;
        strncpy(rows[nrows].src,   src, 255);
        strncpy(rows[nrows].state, st,  31);
        rows[nrows].score = sqlite3_column_double(stmt, 1);
        rows[nrows].pj    = sqlite3_column_int(stmt, 3);
        rows[nrows].pq    = sqlite3_column_int(stmt, 4);
        nrows++;
    }
    sqlite3_finalize(stmt);

    for (int i = 0; i < nrows; i++) {
        /* Skip if active signal in window */
        const char *chk_sql =
            "SELECT 1 FROM signal_window WHERE source=? AND epoch>? LIMIT 1;";
        sqlite3_stmt *chk;
        int active = 0;
        if (sqlite3_prepare_v2(db, chk_sql, -1, &chk, NULL) == SQLITE_OK) {
            sqlite3_bind_text(chk, 1, rows[i].src, -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(chk, 2, (sqlite3_int64)active_cutoff);
            active = (sqlite3_step(chk) == SQLITE_ROW);
            sqlite3_finalize(chk);
        }
        if (active) continue;

        double new_score = rows[i].score * DECAY_FACTOR;
        if (new_score < 0.5) new_score = 0.0;

        const char *new_state = score_to_state(new_score);
        int old_rank = state_rank(rows[i].state);
        int new_rank = state_rank(new_state);

        if (new_rank < old_rank)
            slog("INFO", "DE-ESCALATE src=%s %s→%s score %.2f→%.2f",
                 rows[i].src, rows[i].state, new_state,
                 rows[i].score, new_score);

        const char *upd =
            "UPDATE threat_scores SET score=?,verdict_state=?,last_updated=? "
            "WHERE source=?;";
        sqlite3_stmt *ustmt;
        if (sqlite3_prepare_v2(db, upd, -1, &ustmt, NULL) == SQLITE_OK) {
            sqlite3_bind_double(ustmt, 1, new_score);
            sqlite3_bind_text(ustmt, 2, new_state, -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(ustmt, 3, (sqlite3_int64)now);
            sqlite3_bind_text(ustmt, 4, rows[i].src, -1, SQLITE_TRANSIENT);
            sqlite3_step(ustmt);
            sqlite3_finalize(ustmt);
        }
    }

    pthread_mutex_unlock(&db_mutex);
    slog("INFO", "decay tick complete — %d sources processed", nrows);

    /* Checkpoint WAL to keep wal file small */
    sqlite3_exec(db, "PRAGMA wal_checkpoint(TRUNCATE);", NULL, NULL, NULL);
}

/* ── Decay thread ────────────────────────────────────────────────────────── */

static void *decay_thread(void *arg) {
    (void)arg;
    while (running) {
        sleep(DECAY_INTERVAL);
        if (running) decay_tick();
    }
    return NULL;
}

/* ── Handle STATUS query ─────────────────────────────────────────────────── */

static void handle_status(int fd) {
    pthread_mutex_lock(&db_mutex);

    const char *sql =
        "SELECT source,score,verdict_state,prior_jails,prior_quarantines "
        "FROM threat_scores ORDER BY score DESC;";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        char line[256];
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            snprintf(line, sizeof(line), "%s|%.2f|%s|%d|%d\n",
                sqlite3_column_text(stmt, 0),
                sqlite3_column_double(stmt, 1),
                sqlite3_column_text(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_int(stmt, 4));
            write(fd, line, strlen(line));
        }
        sqlite3_finalize(stmt);
    }

    pthread_mutex_unlock(&db_mutex);
    write(fd, "END\n", 4);
}

/* ── Handle one client connection ────────────────────────────────────────── */

static void handle_client(int fd) {
    char buf[BUF_SIZE] = {0};
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n <= 0) { close(fd); return; }

    /* Strip trailing newline */
    buf[strcspn(buf, "\r\n")] = 0;

    char out[BUF_SIZE] = {0};

    /* Parse */
    char f1[256]={0}, f2[256]={0}, f3[64]={0}, f4[512]={0};
    int fields = sscanf(buf, "%255[^|]|%255[^|]|%63[^|]|%511[^\n]",
                        f1, f2, f3, f4);

    if (fields >= 2 && strcmp(f1, "QUERY") == 0) {
        if (strcmp(f2, "STATUS") == 0) {
            handle_status(fd);
            close(fd);
            return;
        }
        if (strcmp(f2, "DECAY") == 0) {
            decay_tick();
            write(fd, "OK\n", 3);
            close(fd);
            return;
        }
        write(fd, "ERR unknown query\n", 18);
        close(fd);
        return;
    }

    if (fields < 3) {
        write(fd, "ERR bad format\n", 15);
        close(fd);
        return;
    }

    double base_weight = atof(f3);
    if (base_weight <= 0) base_weight = 8.0;

    score_signal(f1, f2, base_weight, out, sizeof(out));
    write(fd, out, strlen(out));
    close(fd);
}

/* ── Signal handler ──────────────────────────────────────────────────────── */

static void handle_sig(int sig) {
    (void)sig;
    running = 0;
}

/* ── Main ────────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[]) {
    /* Resolve base_dir from argv[0] location or cwd */
    if (argc > 1 && strcmp(argv[1], "--base") == 0 && argc > 2) {
        strncpy(base_dir, argv[2], sizeof(base_dir)-1);
    } else {
        if (!getcwd(base_dir, sizeof(base_dir))) {
            fprintf(stderr, "scored: getcwd failed\n");
            return 1;
        }
    }

    /* Open log */
    char log_path[512];
    snprintf(log_path, sizeof(log_path), "%s/%s", base_dir, LOG_FILE);
    logfp = fopen(log_path, "a");
    if (!logfp) logfp = stderr;

    slog("INFO", "scored starting — base=%s", base_dir);

    /* Signal handling */
    signal(SIGTERM, handle_sig);
    signal(SIGINT,  handle_sig);
    signal(SIGPIPE, SIG_IGN);

    /* DB */
    if (db_init() != 0) return 1;

    /* Socket */
    char sock_path[512];
    snprintf(sock_path, sizeof(sock_path), "%s/%s", base_dir, SOCKET_NAME);
    unlink(sock_path);  /* remove stale socket */

    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) { slog("ERROR", "socket() failed"); return 1; }

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path)-1);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        slog("ERROR", "bind() failed: %s", strerror(errno));
        return 1;
    }
    chmod(sock_path, 0600);

    if (listen(server_fd, BACKLOG) < 0) {
        slog("ERROR", "listen() failed");
        return 1;
    }

    slog("INFO", "listening on %s", sock_path);

    /* Decay thread */
    pthread_t dtid;
    pthread_create(&dtid, NULL, decay_thread, NULL);

    /* Accept loop */
    while (running) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            if (running) slog("WARN", "accept() failed: %s", strerror(errno));
            continue;
        }
        handle_client(client_fd);
    }

    /* Cleanup */
    running = 0;
    pthread_join(dtid, NULL);
    close(server_fd);
    unlink(sock_path);
    /* PID owned by controller: unlink(pid_path); */
    if (db) sqlite3_close(db);
    if (logfp && logfp != stderr) fclose(logfp);
    slog("INFO", "scored stopped");
    return 0;
}
