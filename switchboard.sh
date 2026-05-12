#!/data/data/com.termux/files/usr/bin/bash
# ═══════════════════════════════════════════════════
#  MiuiserPeruser SWITCHBOARD — daemon control panel
# ═══════════════════════════════════════════════════

BASE="/data/data/com.termux/files/home/MiuiserPeruser"
BIN="$BASE/bin"
PIDS="$BASE/pipes/pids"
LOG="$BASE/Log_Cabin/switchboard.log"

DAEMONS=(krangd turtlecomd tigerclawd rocksteadyd ratkingd
         rahzerd leatherheadd fugitoidd shredderd burned
         granitord bebopd)

# ── Logging ──────────────────────────────────────────────────
sb_log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] [switchboard] $*" >> "$LOG"
}

# ── PID check ────────────────────────────────────────────────
daemon_pid() {
    local name="$1"
    local pidfile="$PIDS/${name}.pid"
    [[ -f "$pidfile" ]] || return 1
    local pid
    pid=$(cat "$pidfile" 2>/dev/null)
    [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null && echo "$pid"
}

daemon_running() {
    [[ -n "$(daemon_pid "$1")" ]]
}

# ── Start ─────────────────────────────────────────────────────
daemon_start() {
    local name="$1"
    if daemon_running "$name"; then
        echo "  [SWITCHBOARD] $name already running (PID $(daemon_pid "$name"))"
        return
    fi
    if [[ ! -x "$BIN/$name" ]]; then
        echo "  [SWITCHBOARD] $name binary not found in $BIN"
        return 1
    fi
    "$BIN/$name" & disown
    sb_log "START $name"
    echo "  [SWITCHBOARD] $name started"
}

# ── Stop ──────────────────────────────────────────────────────
daemon_stop() {
    local name="$1"
    local pid
    pid=$(daemon_pid "$name")
    if [[ -z "$pid" ]]; then
        echo "  [SWITCHBOARD] $name not running"
        return
    fi
    kill -TERM "$pid" 2>/dev/null
    sleep 1
    kill -0 "$pid" 2>/dev/null && kill -KILL "$pid" 2>/dev/null
    rm -f "$PIDS/${name}.pid"
    sb_log "STOP $name"
    echo "  [SWITCHBOARD] $name stopped"
}

# ── Status ────────────────────────────────────────────────────
daemon_status() {
    local name="$1"
    local pid
    pid=$(daemon_pid "$name")
    if [[ -n "$pid" ]]; then
        echo "  ✓ $name  (PID $pid)"
    else
        echo "  ✗ $name  (offline)"
    fi
}

# ── Fleet commands ────────────────────────────────────────────
fleet_start()  { for d in "${DAEMONS[@]}"; do daemon_start  "$d"; done; }
fleet_stop()   { for d in "${DAEMONS[@]}"; do daemon_stop   "$d"; done; }
fleet_status() {
    echo ""
    echo "  ╔══ SYNDICATE FLEET ══════════════════╗"
    for d in "${DAEMONS[@]}"; do daemon_status "$d"; done
    echo "  ╚═════════════════════════════════════╝"
    echo ""
}

# ── Usage ─────────────────────────────────────────────────────
usage() {
    echo ""
    echo "  SWITCHBOARD — MiuiserPeruser daemon control"
    echo ""
    echo "  Usage: switchboard.sh <command> [daemon]"
    echo ""
    echo "  Commands:"
    echo "    start   <daemon>   Start a single daemon"
    echo "    stop    <daemon>   Stop a single daemon"
    echo "    restart <daemon>   Restart a single daemon"
    echo "    status  [daemon]   Show status (all if no daemon given)"
    echo "    up                 Start entire fleet"
    echo "    down               Stop entire fleet"
    echo ""
    echo "  Daemons: ${DAEMONS[*]}"
    echo ""
}

# ── Entry point ───────────────────────────────────────────────
case "$1" in
    start)   [[ -n "$2" ]] && daemon_start  "$2" || fleet_start ;;
    stop)    [[ -n "$2" ]] && daemon_stop   "$2" || fleet_stop  ;;
    restart) daemon_stop "$2"; sleep 1; daemon_start "$2" ;;
    status)  [[ -n "$2" ]] && daemon_status "$2" || fleet_status ;;
    up)      fleet_start  ;;
    down)    fleet_stop   ;;
    *)       usage ;;
esac
