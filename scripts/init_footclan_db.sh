#!/data/data/com.termux/files/usr/bin/bash
# Initialize the footclan database schema

DB_PATH="${1:-~/MiuiserPeruser/logs/syndicate_footclan.db}"

sqlite3 "$DB_PATH" << 'EOFSQL'
CREATE TABLE IF NOT EXISTS events (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  timestamp TEXT DEFAULT CURRENT_TIMESTAMP,
  source TEXT NOT NULL,
  type TEXT NOT NULL,
  payload TEXT
);
EOFSQL

echo "✅ Database initialized at $DB_PATH"
