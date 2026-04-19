#!/data/data/com.termux/files/usr/bin/bash

STATE_FILE="$HOME/MiuiserPeruser/state/court.events"
LOCK_FILE="$HOME/MiuiserPeruser/src/core/turtlepower.lock"
OUT_FILE="$HOME/MiuiserPeruser/state/court.events.closed"

echo "[TURTLEPOWER] Using lock: $LOCK_FILE"

if [ ! -f "$LOCK_FILE" ]; then
    echo "[TURTLEPOWER] LOCK MISSING — aborting close operation"
    exit 1
fi

echo "[TURTLEPOWER] Closing active case stream..."

tail -n 50 "$STATE_FILE" | while IFS= read -r line; do
    echo "$line|CLOSED"
done >> "$OUT_FILE"

echo "[TURTLEPOWER] Case stream sealed."
