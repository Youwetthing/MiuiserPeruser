#!/data/data/com.termux/files/usr/bin/bash
# MiuiserPeruser - runtime environment
# Source this at the top of every script: source "$(dirname "$0")/../../env.sh"

BASE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Directories
PIPES="$BASE/pipes"
STATE="$BASE/state"
LOGS="$BASE/logs"
BIN="$BASE/bin"

# Pipes (per convention: sockets in pipes/, pids in pipes/pids/)
SUPERHERO_SOCK="$PIPES/superhero.sock"
JUDGEMENT_PIPE="$PIPES/judgement.pipe"
EXECUTION_PIPE="$PIPES/execution.pipe"
INGEST_PIPE="$PIPES/ingest.pipe"

# Logs
TURTLEPOWER_LOG="$BASE/src/turtle_power/logs/turtlepower.log"
SYNDICATE_FEED_SOCK="$PIPES/syndicate_feed.sock"

# Shared helpers (run_shell, _rotate_log, db_log_* stubs)
[ -f "$BASE/lib/miuiserperuser_common.sh" ] && . "$BASE/lib/miuiserperuser_common.sh"
