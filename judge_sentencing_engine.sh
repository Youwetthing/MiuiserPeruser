#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"
REG="$BASE/state/court.registry"
EVT="$BASE/state/court.events"

mkdir -p "$BASE/state"

restart_daemon() {
    name="$1"

    pkill -f "$name" 2>/dev/null

    # naive restart (adjust paths if needed)
    case "$name" in
        turtlepower)
            nohup bash "$BASE/turtlepower_daemon.sh" >/dev/null 2>&1 &
            ;;
    esac

    echo "$(date +%s)|JUDGE|RESTART|$name" >> "$EVT"
}

isolate_daemon() {
    name="$1"
    pkill -f "$name" 2>/dev/null
    echo "$(date +%s)|JUDGE|ISOLATE|$name" >> "$EVT"
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
