#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"
REG="$BASE/state/court.registry"
EVT="$BASE/state/court.events"

echo "⚖️ STRESS TEST STARTING"

# simulate CPU + IO pressure
for i in $(seq 1 20); do
    echo "STRESS|cycle $i|spawning load" >> "$EVT"

    # lightweight CPU burn (safe-ish)
    (yes > /dev/null &)
    (yes > /dev/null &)

    sleep 1

    # kill spawned load gradually
    pkill yes 2>/dev/null
done

echo "STRESS|complete|system stabilising" >> "$EVT"
echo "⚖️ STRESS TEST COMPLETE"
