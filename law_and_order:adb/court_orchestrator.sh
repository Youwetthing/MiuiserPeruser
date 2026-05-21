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
LAW="$BASE/law_and_order:adb"
REG="$BASE/state/court.registry"
EVT="$BASE/state/court.events"

source "$LAW/court_event_lib.sh"

echo "⚖️ COURT ORCHESTRATOR ONLINE"

while true; do

    # 1. Health check — backgrounded so it never blocks the loop
    bash "$LAW/court_health.sh" >> "$BASE/logs/orchestrator_health.log" 2>&1 &

    # 2. Check registry for stopped daemons
    while IFS='|' read -r name state pid; do
        [ "$name" = "# NAME" ] && continue
        [ -z "$name" ] && continue

        if [ "$state" = "STOPPED" ]; then
            emit_event "ORCHESTRATOR" "STOPPED_DETECTED" "$name"
            # Sentencing engine runs on its own loop — just emit, don't call directly
        fi

    done < "$REG"

    sleep 5

done
