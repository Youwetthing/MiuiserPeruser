#!/data/data/com.termux/files/usr/bin/bash
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../env.sh"

BASE="$BASE"
LAW="$BASE/law_and_order:adb"
REG="$BASE/state/court.registry"
EVT="$BASE/state/court.events"

mkdir -p "$BASE/state"

source "$LAW/judicial_firewall.sh"
source "$LAW/court_registry_lib.sh"

declare -A DAEMON_SCRIPTS=(
    [court_core_engine]="$LAW/court_core_engine.sh"
    [court_orchestrator]="$LAW/court_orchestrator.sh"
    [april_o_neil]="$LAW/cre/april_o_neil.sh"
    [escalation]="$LAW/escalation_daemon.sh"
    [visitors_pass]="$LAW/visitors_pass_daemon.sh"
    [turtlepower]="$LAW/turtlepower_daemon.sh"
    [judge_executor]="$LAW/judge_executor.sh"
)

restart_daemon() {
    name="$1"
    allow_action RESTART || return 1

    pid=$(grep "^$name|" "$REG" | cut -d'|' -f3)
    [ -n "$pid" ] && kill "$pid" 2>/dev/null

    script="${DAEMON_SCRIPTS[$name]}"
    if [ -n "$script" ] && [ -f "$script" ]; then
        nohup bash "$script" >> "$BASE/logs/judicial_controller.log" 2>&1 &
        register "$name" RUNNING "$!"
    else
        echo "⚠️ No script for $name" >> "$EVT"
    fi

    (flock -x 200; echo "$(date +%s)|JUDGE|RESTART|$name" >> "$EVT") 200>"$EVT.lock"
}

isolate_daemon() {
    name="$1"
    allow_action ISOLATE || return 1

    pid=$(grep "^$name|" "$REG" | cut -d'|' -f3)
    [ -n "$pid" ] && kill "$pid" 2>/dev/null
    (flock -x 200; echo "$(date +%s)|JUDGE|ISOLATE|$name" >> "$EVT") 200>"$EVT.lock"
}

echo "⚖️ SENTENCING ENGINE ACTIVE"

while true; do
    while IFS='|' read -r name state pid; do
        [ "$name" = "# NAME" ] && continue

        # SENTENCING RULES
        if [ "$state" = "STOPPED" ]; then
            echo "⚖️ SENTENCE: RESTART $name"
            restart_daemon "$name"
        fi

        # fake safety threshold (extend later with real metrics)
        if [ "$name" = "turtlepower" ] && [ "$state" = "UNKNOWN" ]; then
            echo "⚖️ SENTENCE: ISOLATE $name"
            isolate_daemon "$name"
        fi

    done < "$REG"

    sleep 3
done
