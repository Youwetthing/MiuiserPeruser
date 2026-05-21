#!/data/data/com.termux/files/usr/bin/bash
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../env.sh"
MAX_LOG_BYTES=524288  # 500KB

_rotate_log() {
    local f="${1:-$LOG}"
    if [ -f "$f" ] && [ "$(stat -c%s "$f" 2>/dev/null || echo 0)" -gt "${MAX_LOG_BYTES:-524288}" ]; then
        mv "$f" "${f}.1"
        > "$f"
    fi
}


BASE="$BASE"
REG="$BASE/state/court.registry"
EVT="$BASE/state/court.events"

echo "⚖️ STRESS TEST STARTING"
pids=()

# simulate CPU + IO pressure
for i in $(seq 1 20); do
    echo "STRESS|cycle $i|spawning load" >> "$EVT"

    # lightweight CPU burn — tracked PIDs only
    yes > /dev/null & pids+=($!)
    yes > /dev/null & pids+=($!)

    sleep 1

    kill "${pids[@]}" 2>/dev/null
    pids=()
done

echo "STRESS|complete|system stabilising" >> "$EVT"
echo "⚖️ STRESS TEST COMPLETE"
