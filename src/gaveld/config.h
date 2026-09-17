#ifndef GAVELD_CONFIG_H
#define GAVELD_CONFIG_H

/* ── Binary identity ─────────────────────────────────────────────────────── */
#define GAVELD_VERSION          "1.0.0"
#define GAVELD_NAME             "gaveld"

/* ── Base path ───────────────────────────────────────────────────────────── */
/* Overridable at compile time (-DBASE_DIR=...) so the unit tests can point
   the pipe/db/log/state paths at a scratch directory. */
#ifndef BASE_DIR
#define BASE_DIR                "/data/data/com.termux/files/home/MiuiserPeruser"
#endif

/* ── Pipes / sockets ─────────────────────────────────────────────────────── */
#define INGEST_PIPE             BASE_DIR "/pipes/ingest.pipe"
#define GAVELD_SOCK             BASE_DIR "/pipes/gaveld.sock"
#define PID_FILE                BASE_DIR "/pipes/pids/gaveld.pid"

/* ── Database ────────────────────────────────────────────────────────────── */
#define DB_PATH                 BASE_DIR "/state/gaveld.db"

/* ── Logs ────────────────────────────────────────────────────────────────── */
#define LOG_PATH                BASE_DIR "/logs/gaveld.log"
#define LOG_MAX_BYTES           (1 * 1024 * 1024)   /* 1MB before rotation */

/* ── State files ─────────────────────────────────────────────────────────── */
#define SOVEREIGNTY_LIST        BASE_DIR "/state/sovereignty.list"
#define ENFORCE_ALLOWLIST       BASE_DIR "/state/enforce_allowlist"
#define IA_LOCK                 BASE_DIR "/state/internal_affairs.lock"
#define CONSENT_QUEUE           BASE_DIR "/state/consent_queue.state"

/* ── Scoring thresholds ──────────────────────────────────────────────────── */
#define THRESH_JAILED           80.0
#define THRESH_HOUSE_ARREST     70.0
#define THRESH_QUARANTINED      50.0
#define THRESH_WARNED           40.0
#define THRESH_WATCHED          20.0
#define MAX_SCORE               100.0
#define SCORE_FLOOR               0.0

/* ── Decay ───────────────────────────────────────────────────────────────── */
#define DECAY_INTERVAL_SEC      120
#define DECAY_FACTOR            0.88
#define SIGNAL_WINDOW_SEC       120

/* ── Consent timeouts (seconds) ──────────────────────────────────────────── */
#define CONSENT_TIMEOUT_JAILED          0       /* wait indefinitely */
#define CONSENT_TIMEOUT_HOUSE_ARREST    1800    /* 30 min → hold, re-notify */
#define CONSENT_TIMEOUT_QUARANTINED     600     /* 10 min → hold, re-notify */
#define CONSENT_TIMEOUT_WARNED          600     /* 10 min → auto-proceed */
#define CONSENT_JAILED_HARD_LIMIT       86400   /* 24h max wait, then hold */
#define CONSENT_RENOTIFY_INTERVAL       600     /* re-notify every 10 min in hold */

/* ── Ethical floors ──────────────────────────────────────────────────────── */
#define FLOOR_KILL_MIN_SOURCES          2       /* distinct daemon sources for KILL */
#define FLOOR_CLEAN_SIGNAL_MAX_AGE_SEC  60      /* max signal age for CLEAN enforcement */
#define FLOOR_JAILED_REQUIRES_PRIOR     1       /* must have HOUSE_ARREST/QUARANTINED first */

/* ── Audit ───────────────────────────────────────────────────────────────── */
#define AUDIT_INTERVAL_SEC      300     /* 5 minutes */
#define AUDIT_FLAG_WINDOW_SEC   3600    /* 1 hour window for repeat flag detection */
#define AUDIT_REPEAT_THRESHOLD  3       /* flags before notification */
#define AUDIT_VERDICT_RATE_MAX  30      /* % JAILED verdicts before flag */
#define AUDIT_SCORE_INFLATION   60.0    /* avg score threshold before flag */
#define AUDIT_STALL_SEC         300     /* case stuck > 5min = stall */

/* ── Covariance multipliers ──────────────────────────────────────────────── */
#define COV_INTEGRITY_VIOLATION         1.80
#define COV_NETWORK_CPU                 1.60
#define COV_WAKELOCK_NETWORK            1.50
#define COV_THERMAL_CPU_CRITICAL        1.40
#define COV_THERMAL_NETWORK             1.30
#define COV_CPU_WAKELOCK                1.20
#define COV_STACK_BONUS                 1.20    /* >= 3 distinct signals */
#define COV_STACK_MIN_SIGNALS           3

/* ── Tier modifiers ──────────────────────────────────────────────────────── */
#define TIER_MOD_OWN_DAEMON     0.40
#define TIER_MOD_SOVEREIGNTY    0.15
#define TIER_MOD_MIUI_AOSP      0.60
#define TIER_MOD_UNKNOWN        1.00

/* ── Recidivism multipliers ──────────────────────────────────────────────── */
#define RECID_MULTI_JAIL        2.00    /* >= 2 prior jails */
#define RECID_SINGLE_JAIL       1.60    /* 1 prior jail */
#define RECID_QUARANTINE        1.30    /* prior quarantine, no jail */
#define RECID_NONE              1.00

/* ── Enforce ─────────────────────────────────────────────────────────────── */
#define RISH_PATH               BASE_DIR "/rish"
#define ENFORCE_CMD_KILL        "am force-stop %s"
#define ENFORCE_CMD_DISABLE     "pm disable-user --user 0 %s"
#define ENFORCE_CMD_RESTRICT    "am kill %s"

/* ── Thread queue sizes ──────────────────────────────────────────────────── */
#define INGEST_QUEUE_SIZE       256
#define VERDICT_QUEUE_SIZE      64
#define CONSENT_QUEUE_SIZE      32
#define ENFORCE_QUEUE_SIZE      32

/* ── Misc ────────────────────────────────────────────────────────────────── */
#define BACKLOG                 32
#define BUF_SIZE                2048
#define MAX_SOURCE_LEN          256
#define MAX_SIGNAL_LEN          64
#define MAX_CTX_LEN             512
#define MAX_STATE_LEN           32

#endif /* GAVELD_CONFIG_H */
