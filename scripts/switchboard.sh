#!/data/data/com.termux/files/usr/bin/bash
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../env.sh"
# ═══════════════════════════════════════════════════════════
#  MiuiserPeruser SWITCHBOARD v1.1
#  Daemon control panel — start/stop/status + last state
# ═══════════════════════════════════════════════════════════

BASE="$BASE"
BIN="$BASE/bin"
PIDS="$BASE/pipes/pids"
STATE="$BASE/pipes/state"
LOG="$BASE/Log_Cabin/switchboard.log"

DAEMONS=(krangd turtlecomd splinterd tigerclawd rocksteadyd
         ratkingd rahzerd leatherheadd fugitoidd shredderd
         burned granitord bebopd metalheadd)

# ── Colours ──────────────────────────────────────────────────
RED='\033[0;31m'
GRN='\033[0;32m'
YLW='\033[0;33m'
CYN='\033[0;36m'
BLD='\033[1m'
RST='\033[0m'

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

# ── Last known state ──────────────────────────────────────────
daemon_last_state() {
    local name="$1"
    local statefile="$STATE/${name}.last"
    if [[ -f "$statefile" ]]; then
        cat "$statefile"
    else
        echo "  no state recorded yet"
    fi
}

# ── Start ─────────────────────────────────────────────────────
daemon_start() {
    local name="$1"
    if daemon_running "$name"; then
        echo -e "  ${YLW}~${RST} $name already running (PID $(daemon_pid "$name"))"
        return
    fi
    if [[ ! -x "$BIN/$name" ]]; then
        echo -e "  ${RED}✗${RST} $name binary not found in $BIN"
        return 1
    fi
    "$BIN/$name" & disown
    sb_log "START $name"
    echo -e "  ${GRN}✓${RST} $name started"
}

# ── Stop ──────────────────────────────────────────────────────
daemon_stop() {
    local name="$1"
    local pid
    pid=$(daemon_pid "$name")
    if [[ -z "$pid" ]]; then
        echo -e "  ${YLW}~${RST} $name not running"
        return
    fi
    kill -TERM "$pid" 2>/dev/null
    sleep 1
    kill -0 "$pid" 2>/dev/null && kill -KILL "$pid" 2>/dev/null
    rm -f "$PIDS/${name}.pid"
    sb_log "STOP $name"
    echo -e "  ${RED}✗${RST} $name stopped"
}

# ── Status ────────────────────────────────────────────────────
daemon_status() {
    local name="$1"
    local pid
    pid=$(daemon_pid "$name")
    if [[ -n "$pid" ]]; then
        echo -e "  ${GRN}✓${RST} ${BLD}$name${RST} (PID $pid)"
    else
        echo -e "  ${RED}✗${RST} ${BLD}$name${RST} (offline)"
    fi
}

# ── Status + last state ───────────────────────────────────────
daemon_inspect() {
    local name="$1"
    daemon_status "$name"
    echo -e "    ${CYN}└─${RST} $(daemon_last_state "$name")"
}

# ── Fleet commands ────────────────────────────────────────────
fleet_start() {
    echo -e "\n${BLD}Starting fleet...${RST}"
    # Start core first
    for d in krangd turtlecomd splinterd; do
        daemon_start "$d"
        sleep 0.5
    done
    # Then syndicate
    for d in tigerclawd rocksteadyd ratkingd rahzerd leatherheadd \
              fugitoidd shredderd burned granitord bebopd metalheadd; do
        daemon_start "$d"
    done
    echo ""
}

fleet_stop() {
    echo -e "\n${BLD}Stopping fleet...${RST}"
    for d in "${DAEMONS[@]}"; do daemon_stop "$d"; done
    echo ""
}

fleet_status() {
    echo ""
    echo -e "  ${BLD}╔══ SYNDICATE FLEET ══════════════════════════╗${RST}"
    echo -e "  ${BLD}║  Core                                       ║${RST}"
    for d in krangd turtlecomd splinterd; do
        daemon_status "$d"
    done
    echo -e "  ${BLD}║  Syndicate                                  ║${RST}"
    for d in tigerclawd rocksteadyd ratkingd rahzerd leatherheadd \
              fugitoidd shredderd burned granitord bebopd metalheadd; do
        daemon_status "$d"
    done
    echo -e "  ${BLD}╚═════════════════════════════════════════════╝${RST}"
    echo ""
}

fleet_inspect() {
    echo ""
    echo -e "  ${BLD}╔══ SYNDICATE LAST STATE ═════════════════════╗${RST}"
    for d in "${DAEMONS[@]}"; do
        daemon_inspect "$d"
    done
    echo -e "  ${BLD}╚═════════════════════════════════════════════╝${RST}"
    echo ""
}

# ── Usage ─────────────────────────────────────────────────────
usage() {
    echo ""
    echo -e "  ${BLD}SWITCHBOARD${RST} — MiuiserPeruser daemon control"
    echo ""
    echo "  Usage: switchboard.sh <command> [daemon]"
    echo ""
    echo "  Commands:"
    echo "    start   <daemon>   Start a single daemon"
    echo "    stop    <daemon>   Stop a single daemon"
    echo "    restart <daemon>   Restart a single daemon"
    echo "    status  [daemon]   Show running status"
    echo "    inspect [daemon]   Status + last known state"
    echo "    up                 Start entire fleet (core first)"
    echo "    down               Stop entire fleet"
    echo "    state              Show last state for all daemons"
    echo ""
    echo -e "  Daemons: ${CYN}${DAEMONS[*]}${RST}"
    echo ""
}

# ── Entry point ───────────────────────────────────────────────
case "$1" in
    start)
        [[ -n "$2" ]] && daemon_start "$2" || fleet_start ;;
    stop)
        [[ -n "$2" ]] && daemon_stop "$2" || fleet_stop ;;
    restart)
        [[ -n "$2" ]] || { usage; exit 1; }
        daemon_stop "$2"; sleep 1; daemon_start "$2" ;;
    status)
        [[ -n "$2" ]] && daemon_status "$2" || fleet_status ;;
    inspect)
        [[ -n "$2" ]] && daemon_inspect "$2" || fleet_inspect ;;
    up)
        fleet_start ;;
    down)
        fleet_stop ;;
    state)
        fleet_inspect ;;
    *)
        usage ;;
esac
