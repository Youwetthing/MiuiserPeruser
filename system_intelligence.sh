#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"
REG="$BASE/state/court.registry"

while true; do

total=0
alive=0

while IFS='|' read -r name state pid; do
    [ -z "$name" ] && continue
    [ "$name" = "# NAME" ] && continue
    total=$((total+1))

    [ "$state" = "RUNNING" ] && alive=$((alive+1))
done < "$REG"

if [ "$total" -eq 0 ]; then
    echo "🧠 SYSTEM INTELLIGENCE: UNKNOWN"
    sleep 5
    exit
fi

score=$((alive * 100 / total))

echo "🧠 SYSTEM INTELLIGENCE SCORE: $score"
echo ""

if [ "$score" -ge 85 ]; then
    echo "🟢 STABLE SYSTEM"
elif [ "$score" -ge 60 ]; then
    echo "🟠 DEGRADED SYSTEM"
else
    echo "🔴 CRITICAL INSTABILITY"
fi

echo ""
echo "🔁 refresh: 10s"
sleep 10

done
