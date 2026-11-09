#!/usr/bin/env bash
# watchdog.sh - keep the core four alive (gaveld governs; the rest feed it)
BASE="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$BASE/bin"; LOG="$BASE/logs"
CORE="gaveld splinterd turtlecomd krangd"
INTERVAL=15
cd "$BASE" || exit 1
mkdir -p "$LOG" pipes/pids
running() { ps -eo args | awk -v n="$1" '{b=$1; sub(".*/","",b); if (b==n) f=1} END{exit !f}'; }
while :; do
  for n in $CORE; do
    if ! running "$n"; then
      echo "$(date '+%F %T') restart $n" >> "$LOG/watchdog.log"
      setsid nohup "$BIN/$n" >> "$LOG/$n.log" 2>&1 &
    fi
  done
  sleep "$INTERVAL"
done
