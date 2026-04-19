#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"
REG="$BASE/state/court.registry"
EVT="$BASE/state/court.events"

mkdir -p "$BASE/state"
mkdir -p "$BASE/logs"

register() {
    name="$1"; state="$2"; pid="$3"
    grep -v "^$name|" "$REG" > "$REG.tmp" 2>/dev/null
    mv "$REG.tmp" "$REG" 2>/dev/null
    echo "$name|$state|$pid" >> "$REG"
}

emit() {
    echo "$(date +%s)|turtlepower|$1|$2" >> "$EVT"
}

echo "[TURTLEPOWER] BOOT" >> "$BASE/logs/turtlepower.log"

register "turtlepower" "RUNNING" "$$"
emit "STATE" "RUNNING"

trap 'register turtlepower STOPPED $$; emit STATE STOPPED; exit' EXIT INT TERM

while true; do
    emit "HEARTBEAT" "OK"
    sleep 5
done
