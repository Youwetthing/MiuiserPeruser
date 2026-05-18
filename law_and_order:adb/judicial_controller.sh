#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"
LAW="$BASE/law_and_order:adb"
REG="$BASE/state/court.registry"
LOG="$BASE/logs/judicial_controller.log"
PID_DIR="$BASE/pipes/pids"
LOCK="$BASE/state/turtlepower.lock"

mkdir -p "$BASE/logs" "$PID_DIR"

# ── Daemon registry (lookup by name) ─────────────────────────────────────────

declare -A DAEMON_SCRIPTS=(
    [court_core_engine]="$LAW/court_core_engine.sh"
    [court_orchestrator]="$LAW/court_orchestrator.sh"
    [court_dispatcher]="$LAW/court_dispatcher.sh"
    [april_o_neil]="$LAW/cre/april_o_neil.sh"
    [escalation]="$LAW/escalation_daemon.sh"
    [visitors_pass]="$LAW/visitors_pass_daemon.sh"
    [turtlepower]="$LAW/turtlepower_daemon.sh"
    [baxter]="$BASE/scripts/baxter_stockman.sh"
    [superhero]="$LAW/superhero_adapter.sh"
    [judge_executor]="$LAW/judge_executor.sh"
    # v2 additions
    [parole_engine]="$LAW/parole_engine.sh"
    [internal_affairs]="$LAW/internal_affairs.sh"
)

# ── Ordered launch sequence ───────────────────────────────────────────────────
# ORDER MATTERS. Do not use ${!DAEMON_SCRIPTS[@]} for start/stop —
# associative arrays have no guaranteed iteration order in bash.

LAUNCH_ORDER=(
    court_core_engine
    court_orchestrator
    court_dispatcher
    april_o_neil
    escalation
    visitors_pass
    turtlepower
    baxter
    superhero
    parole_engine      # v2: must be before judge_executor
    judge_executor
    internal_affairs   # v2: always last — reads all others' state
)

# Reverse for shutdown
STOP_ORDER=(
    internal_affairs
    judge_executor
    parole_engine
    superhero
    baxter
    turtlepower
    visitors_pass
    escalation
    april_o_neil
    court_dispatcher
    court_orchestrator
    court_core_engine
)

# ── v2 state file initialisation ─────────────────────────────────────────────

_init_v2_state_files() {
    local STATE_DIR="$BASE/state"
    local CRE_DIR="$BASE/cre"

    touch "${STATE_DIR}/threat_scores.state" \
          "${STATE_DIR}/signal_window.state" \
          "${STATE_DIR}/consent_queue.state"

    mkdir -p "${STATE_DIR}/visitors_pass" \
             "${STATE_DIR}/criminal_record"

    touch "${STATE_DIR}/visitors_pass/pass_registry" \
          "${STATE_DIR}/criminal_record/ledger.log" \
          "${STATE_DIR}/criminal_record/ledger.log.lock"

    if [[ ! -f "${STATE_DIR}/sovereignty.list" ]]; then
        cat > "${STATE_DIR}/sovereignty.list" << 'EOF'
# MiuiserPeruser Sovereignty List
# Format: package.name|PROTECTED|reason|added_epoch
# Managed by sovereignty tool (not yet built).
EOF
    fi

    touch "${CRE_DIR}/scoring_reasoning.log" \
          "${CRE_DIR}/internal_affairs.log"

    echo "$(date +%s)|judicial_controller|V2_STATE_INIT|ok" >> "$BASE/state/court.events"
}

# ── Court status ──────────────────────────────────────────────────────────────

court_status() {
    case "$1" in
        online)   echo "⚖️  Court is in session" ;;
        offline)  echo "🧾 Your case is dismissed" ;;
        starting) echo "⚖️  Court is convening" ;;
        stopping) echo "⚖️  Court is adjourned" ;;
    esac
}

# ── Daemon control ────────────────────────────────────────────────────────────

start_daemon() {
    local name="$1"
    local script="${DAEMON_SCRIPTS[$name]}"
    local pidfile="$PID_DIR/${name}.pid"

    if [[ -z "$script" ]]; then
        echo "  [$name] ERROR: not in DAEMON_SCRIPTS"
        return 1
    fi

    if [[ ! -x "$script" ]]; then
        echo "  [$name] ERROR: ${script} not executable or missing"
        return 1
    fi

    if [[ -f "$pidfile" ]] && kill -0 "$(cat "$pidfile")" 2>/dev/null; then
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

    if [[ -f "$pidfile" ]]; then
        local pid
        pid=$(cat "$pidfile")
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
    for name in "${LAUNCH_ORDER[@]}"; do
        local pidfile="$PID_DIR/${name}.pid"
        if [[ -f "$pidfile" ]] && kill -0 "$(cat "$pidfile")" 2>/dev/null; then
            echo "  [$name] RUNNING ($(cat "$pidfile"))"
        else
            echo "  [$name] STOPPED"
        fi
    done
}

# ── Entrypoint ────────────────────────────────────────────────────────────────

case "$1" in
    start)
        court_status starting
        _init_v2_state_files
        [[ -f "$LOCK" ]] && sed -i 's/^LOCK_STATE=.*/LOCK_STATE=ACTIVE/' "$LOCK"
        for name in "${LAUNCH_ORDER[@]}"; do
            start_daemon "$name"
            sleep 0.3   # brief stagger — avoids pipe race on mkfifo
        done
        ;;
    stop)
        court_status stopping
        [[ -f "$LOCK" ]] && sed -i 's/^LOCK_STATE=.*/LOCK_STATE=INACTIVE/' "$LOCK"
        for name in "${STOP_ORDER[@]}"; do
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
