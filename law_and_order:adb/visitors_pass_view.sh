#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"
EVT="$BASE/state/court.events"

MODE="VISITOR"  # could later expand to JUDGE / FULL / VISITOR

echo "🎟️ COURT VISITORS PASS VIEW"
echo "=========================="
echo "Mode: READ-ONLY (filtered visibility)"
echo ""

while true; do
    clear
    echo "🎟️ LIVE COURT FEED (VISITORS PASS)"
    echo "=================================="
    date
    echo ""

    tail -n 20 "$EVT" 2>/dev/null | while IFS='|' read -r ts src type payload; do

        # VISITOR FILTER RULES
        case "$type" in
            HEARTBEAT)
                echo "• System alive: $src"
                ;;
            CASE)
                echo "• New case filed"
                ;;
            INTEL)
                echo "• System health update"
                ;;
            SANDBOX)
                echo "• Containment action executed"
                ;;
            JAIL)
                echo "• Process detained"
                ;;
            *)
                echo "• Event: $src → $type"
                ;;
        esac
    done

    echo ""
    echo "🔁 refresh: 8s (VISITOR MODE)"
    sleep 8
done
