#!/data/data/com.termux/files/usr/bin/bash
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../env.sh"
# ============================================================
# turtlepower_daemon.sh
# Owns two child processes:
#   1. turtlepower_core.sh  — infinite pipe listener (decision layer)
#   2. turtlepower_engine.sh — periodic rule evaluator (action layer)
#
# turtlepower.lock gates startup: LOCK_STATE must be ACTIVE.
# ============================================================

BASE="$BASE"
LAW="$BASE/law_and_order:adb"
TP_DIR="$BASE/src/turtle_power"
CORE_SCRIPT="$TP_DIR/turtlepower_core.sh"
ENGINE_SCRIPT="$BASE/src/core/turtlepower_engine.sh"
LOCK="$BASE/state/turtlepower.lock"
PIDDIR="$BASE/state/pids"
LOG="$TP_DIR/logs/turtlepower_daemon.log"
CORE_PID_FILE="$PIDDIR/turtlepower_core.pid"

EVAL_INTERVAL=10   # seconds between engine rule evaluations

mkdir -p "$TP_DIR/logs" "$PIDDIR"

log() { echo "[TP_DAEMON] $(date +%s) $1" | tee -a "$LOG"; }

# -------------------------
# LOCK CHECK
# -------------------------
read_lock_val() {
    grep "^$1=" "$LOCK" 2>/dev/null | cut -d'=' -f2
}

lock_state=$(read_lock_val LOCK_STATE)
if [ "$lock_state" != "ACTIVE" ]; then
    log "LOCK_STATE='$lock_state' (expected ACTIVE). Refusing to start."
    echo "⚠️  Turtlepower lock not ACTIVE — set LOCK_STATE=ACTIVE in state/turtlepower.lock"
    exit 1
fi

log "Lock ACTIVE — starting subsystems"

# -------------------------
# LAUNCH CORE (pipe listener)
# -------------------------
start_core() {
    nohup bash "$CORE_SCRIPT" >> "$LOG" 2>&1 &
    local pid=$!
    echo "$pid" > "$CORE_PID_FILE"
    log "turtlepower_core started (PID $pid)"
}

start_core

# -------------------------
# MAIN LOOP — periodic engine + core watchdog
# -------------------------
log "Entering evaluation loop (interval: ${EVAL_INTERVAL}s)"

while true; do
    # Watchdog: restart core if it died
    if [ -f "$CORE_PID_FILE" ]; then
        core_pid=$(cat "$CORE_PID_FILE")
        if ! kill -0 "$core_pid" 2>/dev/null; then
            log "WARNING: turtlepower_core (PID $core_pid) died — restarting"
            start_core
        fi
    fi

    # Run one-shot rule engine
    bash "$ENGINE_SCRIPT" >> "$LOG" 2>&1

    sleep "$EVAL_INTERVAL"
done
