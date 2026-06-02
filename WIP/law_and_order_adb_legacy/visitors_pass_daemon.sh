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
VP="$BASE/state/visitors_pass/pass_registry"
EVT="$BASE/state/court.events"

mkdir -p "$(dirname "$VP")"
touch "$VP"

echo "🎟️ VISITORS PASS SYSTEM ONLINE"

while true; do

    now=$(date +%s)

    while IFS='|' read -r name state reason start duration; do

        [ "$name" = "" ] && continue

        expiry=$((start + duration))

        if [ "$now" -gt "$expiry" ]; then
            (flock -x 200; echo "$(date +%s)|VP|EXPIRED|$name" >> "$EVT") 200>"$EVT.lock"

            # enforce re-containment
            pid=$(grep "^$name|" "$BASE/state/court.registry" | cut -d'|' -f3)
            [ -n "$pid" ] && kill "$pid" 2>/dev/null || pkill -f "$name" 2>/dev/null
        fi

    done < "$VP"

    sleep 5
done
