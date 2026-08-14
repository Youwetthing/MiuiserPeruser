#!/data/data/com.termux/files/usr/bin/bash
# init_sewer_db.sh — creates sewer.db (accumulation + rollup schema, v2)
# Only splinterd ever opens this db.

set -e

DB_PATH="${1:-Registry/sewer.db}"
DB_DIR=$(dirname "$DB_PATH")
mkdir -p "$DB_DIR"

sqlite3 "$DB_PATH" <<'EOF'
PRAGMA auto_vacuum = INCREMENTAL;

CREATE TABLE IF NOT EXISTS daemon_sources (
    name    TEXT PRIMARY KEY,
    enabled INTEGER NOT NULL DEFAULT 1
);

CREATE TABLE IF NOT EXISTS emit_state (
    source       TEXT NOT NULL,
    event_type   TEXT NOT NULL,
    first_seen   TEXT NOT NULL,
    last_seen    TEXT NOT NULL,
    repeat_count INTEGER NOT NULL DEFAULT 0,
    last_detail  TEXT,
    PRIMARY KEY (source, event_type)
);

CREATE TABLE IF NOT EXISTS emit_transitions (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    source          TEXT NOT NULL,
    event_type      TEXT NOT NULL,
    transition_type TEXT NOT NULL,   -- 'first_seen' | 'resumed'
    ts              TEXT NOT NULL,
    detail          TEXT
);

CREATE TABLE IF NOT EXISTS daemon_stats (
    name            TEXT PRIMARY KEY,
    total_emitted   INTEGER NOT NULL DEFAULT 0,
    last_event_type TEXT,
    last_event_time TEXT
);

CREATE TABLE IF NOT EXISTS sewer_rats (
    id        INTEGER PRIMARY KEY AUTOINCREMENT,
    rollup_ts TEXT NOT NULL,
    summary   TEXT NOT NULL
);

INSERT OR IGNORE INTO daemon_sources (name) VALUES
    ('krangd'), ('turtlecomd'), ('tigerclawd'), ('rocksteadyd'),
    ('ratkingd'), ('rahzerd'), ('leatherheadd'), ('bebopd'),
    ('fugitoidd'), ('shredderd'), ('burned'), ('granitord'), ('metalheadd');

INSERT OR IGNORE INTO daemon_stats (name, total_emitted)
    SELECT name, 0 FROM daemon_sources;
EOF

echo "sewer.db initialized at $(realpath "$DB_PATH")"
echo "NOTE: this replaces the v1 schema (subscribers/dispatch_log). If Registry/sewer.db"
echo "already exists from the earlier deploy, delete it first — this script won't migrate it."
