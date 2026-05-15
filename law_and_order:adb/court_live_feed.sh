#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"
REG="$BASE/state/court.registry"
EVT="$BASE/state/court.events"

court_status() {
    case "$1" in
        RUNNING) echo "🟢 ACTIVE" ;;
        STOPPED) echo "🔴 STOPPED" ;;
        *) echo "⚪ UNKNOWN" ;;
    esac
}

while true; do
    clear
    echo "⚖️ LIVE COURT FEED"
    echo "=========================="
    date
    echo ""

    echo "📜 SYSTEM STATE"
    echo "--------------------------"

    tail -n +2 "$REG" 2>/dev/null | while IFS='|' read -r name state pid; do
        printf "%-15s %-10s PID:%s\n" "$name" "$(court_status "$state")" "$pid"
    done

    echo ""
    echo "📡 RECENT EVENTS"
    echo "--------------------------"
    tail -n 6 "$EVT" 2>/dev/null

    echo ""
    echo "🔁 refresh: 8s"
    sleep 8
done
