#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"
REG="$BASE/state/court.registry"
EVT="$BASE/state/court.events"

echo "⚖️ JUDGE JUDY LIVE COURTROOM"
echo "=========================="

while true; do
    clear
    echo "⚖️ LIVE VERDICT STREAM"
    echo "=========================="
    date
    echo ""

    echo "📜 REGISTRY VERDICTS:"
    while IFS='|' read -r name state pid; do
        [ "$name" = "# NAME" ] && continue

        if [ "$state" = "RUNNING" ]; then
            verdict="✔ STABLE"
        elif [ "$state" = "STOPPED" ]; then
            verdict="⚠ FAILURE"
        else
            verdict="❓ UNKNOWN"
        fi

        printf "%-15s %-10s (pid:%s)\n" "$name" "$verdict" "$pid"
    done < "$REG"

    echo ""
    echo "📡 RECENT EVENTS:"
    tail -n 6 "$EVT" 2>/dev/null

    echo ""
    echo "🔁 refreshing..."
    sleep 2
done
