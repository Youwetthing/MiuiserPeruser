#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"
REG="$BASE/state/court.registry"
EVT="$BASE/state/court.events"

echo "⚖️ COURT ORCHESTRATOR ONLINE"

while true; do

    # 1. run intelligence check
    bash "$BASE/system_intelligence.sh"

    # 2. check registry for bad states
    while IFS='|' read -r name state pid; do
        [ "$name" = "# NAME" ] && continue

        # simulate sentencing hook
        if [ "$state" = "STOPPED" ]; then
            echo "$(date +%s)|ORCHESTRATOR|RESTART|$name" >> "$EVT"
            bash "$BASE/judge_sentencing_engine.sh" >/dev/null 2>&1 &
        fi

    done < "$REG"

    sleep 5
done
