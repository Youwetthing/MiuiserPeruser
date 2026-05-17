#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"
LAW="$BASE/law_and_order:adb"
REG="$BASE/state/court.registry"
LOG="$BASE/logs/judicial_controller.log"
PID_DIR="$BASE/pipes/pids"
LOCK="$BASE/state/turtlepower.lock"

mkdir -p "$BASE/logs" "$PID_DIR"

declare -A DAEMON_SCRIPTS=(
    [court_core_engine]="$LAW/court_core_engine.sh"
    [court_orchestrator]="$LAW/court_orchestrator.sh"
    [april_o_neil]="$LAW/cre/april_o_neil.sh"
    [escalation]="$LAW/escalation_daemon.sh"
    [visitors_pass]="$LAW/visitors_pass_daemon.sh"
    [turtlepower]="$LAW/turtlepower_daemon.sh"
    [baxter]="$BASE/scripts/baxter_stockman.sh"
    [superhero]="$LAW/superhero_adapter.sh"
    [judge_executor]="$LAW/judge_executor.sh"
    [court_dispatcher]="$LAW/court_dispatcher.sh"
)

court_status() {
    case "$1" in
        online)   echo "⚖️  Court is in session" ;;
        offline)  echo "🧾 Your case is dismissed" ;;
        starting) echo "⚖️  Court is convening" ;;
        stopping) echo "⚖️  Court is adjourned" ;;
    esac
}

start_daemon() {
    local name="$1"
    local script="${DAEMON_SCRIPTS[$name]}"
    local pidfile="$PID_DIR/${name}.pid"

    if [ -f "$pidfile" ] && kill -0 "$(cat "$pidfile")" 2>/dev/null; then
        echo "  [$name] already running ($(cat "$pidfile"))"
        return
    fi

    bash "$script" >> "$LOG" 2>&1 &
    echo $! > "$pidfile"
    source "$LAW/court_registry_lib.sh"
    register "$name" RUNNING "$(cat "$pidfile")"
    echo "  [$name] started ($(cat "$pidfile"))"
    echo "$(date +%s)|judicial_controller|STARTED|$name" >> "$BASE/state/court.events"
}

stop_daemon() {
    local name="$1"
    local pidfile="$PID_DIR/${name}.pid"

    if [ -f "$pidfile" ]; then
        local pid=$(cat "$pidfile")
        kill "$pid" 2>/dev/null && echo "  [$name] stopped"
        rm -f "$pidfile"
        echo "$(date +%s)|judicial_controller|STOPPED|$name" >> "$BASE/state/court.events"
    else
        echo "  [$name] not running"
    fi
}

status_system() {
    echo "=========================="
    court_status online
    echo "=========================="
    echo ""
    echo "📜 COURT REGISTRY:"
    cat "$REG"
    echo ""
    echo "📌 PID FILES:"
    for name in "${!DAEMON_SCRIPTS[@]}"; do
        local pidfile="$PID_DIR/${name}.pid"
        if [ -f "$pidfile" ] && kill -0 "$(cat "$pidfile")" 2>/dev/null; then
            echo "  [$name] RUNNING ($(cat "$pidfile"))"
        else
            echo "  [$name] STOPPED"
        fi
    done
}

case "$1" in
    start)
        court_status starting
        [ -f "$LOCK" ] && sed -i 's/^LOCK_STATE=.*/LOCK_STATE=ACTIVE/' "$LOCK"
        for name in "${!DAEMON_SCRIPTS[@]}"; do
            start_daemon "$name"
        done
        ;;
    stop)
        court_status stopping
        [ -f "$LOCK" ] && sed -i 's/^LOCK_STATE=.*/LOCK_STATE=INACTIVE/' "$LOCK"
        for name in "${!DAEMON_SCRIPTS[@]}"; do
            stop_daemon "$name"
        done
        ;;
    restart)
        bash "$0" stop
        sleep 1
        bash "$0" start
        ;;
    status)
        status_system
        ;;
    *)
        echo "Usage: $0 {start|stop|restart|status}"
        exit 1
        ;;
esac
