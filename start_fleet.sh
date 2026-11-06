#!/usr/bin/env bash
# start_fleet.sh - launch the syndicate fleet from bin/
BASE="$(cd "$(dirname "$0")" && pwd)"
BIN="$BASE/bin"
LOG_DIR="$BASE/logs"
FLEET="splinterd turtlecomd krangd bebopd burned fugitoidd gaveld granitord leatherheadd metalheadd nulld overlordd rahzerd ratkingd rocksteadyd shredderd tigerclawd"

cd "$BASE" || exit 1
mkdir -p "$LOG_DIR" "$BASE/pipes/pids"
termux-wake-lock

echo "Stopping old daemons..."
for name in $FLEET; do
    for pid in $(ps -eo pid,args | awk -v n="$name" '{b=$2; sub(".*/","",b); if (b==n) print $1}'); do
        kill "$pid" 2>/dev/null
    done
done
sleep 2

echo "Launching fleet..."
for name in $FLEET; do
    if [ ! -x "$BIN/$name" ]; then
        echo "skip $name (not built)"
        continue
    fi
    nohup "$BIN/$name" > "$LOG_DIR/$name.log" 2>&1 & disown
    echo "$name -> PID $!"
    sleep 1
done

sleep 2
echo "=== Status ==="
for name in $FLEET; do
    if ps -eo args | awk -v n="$name" '{b=$1; sub(".*/","",b); if (b==n) f=1} END{exit !f}'; then
        echo "up   $name"
    else
        echo "DOWN $name"
    fi
done
