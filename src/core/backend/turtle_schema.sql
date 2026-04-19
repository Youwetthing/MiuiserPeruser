CREATE TABLE IF NOT EXISTS turtle_events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp INTEGER NOT NULL,
    event_type TEXT NOT NULL,
    backend_expected TEXT,
    backend_actual TEXT,
    command TEXT,
    success INTEGER,
    metadata TEXT
);
