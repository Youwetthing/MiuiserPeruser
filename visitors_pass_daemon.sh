#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"
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
            echo "$(date +%s)|VP|EXPIRED|$name" >> "$EVT"

            # enforce re-containment
            pkill -f "$name" 2>/dev/null
        fi

    done < "$VP"

    sleep 5
done
