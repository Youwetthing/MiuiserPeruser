#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"
REG="$BASE/state/court.registry"

total=0
alive=0

while IFS='|' read -r name state pid; do
    [ "$name" = "# NAME" ] && continue
    total=$((total+1))

    if [ "$state" = "RUNNING" ]; then
        alive=$((alive+1))
    fi
done < "$REG"

if [ "$total" -eq 0 ]; then
    echo "System Health: UNKNOWN"
    exit
fi

score=$((alive * 100 / total))

echo "⚖️ SYSTEM HEALTH: ${score}%"

if [ "$score" -ge 90 ]; then
    echo "🟢 STABLE"
elif [ "$score" -ge 60 ]; then
    echo "🟠 DEGRADED"
else
    echo "🔴 CRITICAL"
fi
