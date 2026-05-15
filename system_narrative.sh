#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"
EVT="$BASE/state/court.events"

while true; do
    clear
    echo "📖 SYSTEM NARRATIVE"
    echo "=========================="
    date
    echo ""

    while IFS='|' read -r ts src event detail; do
        [ "$ts" = "# TIME" ] && continue

        case "$event" in
            HEARTBEAT) echo "• Pulse: $src OK" ;;
            STATE) echo "• State change: $src → $detail" ;;
            STRESS) echo "• Load spike: $detail" ;;
            RESTART) echo "• Recovery: $detail restarted" ;;
            ISOLATE) echo "• Isolation: $detail" ;;
            *) echo "• Event: $src | $event | $detail" ;;
        esac
    done < <(tail -n 50 "$EVT")

    echo ""
    echo "🔁 refresh: 10s"
    sleep 10
done
