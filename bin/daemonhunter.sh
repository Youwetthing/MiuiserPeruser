#!/data/data/com.termux/files/usr/bin/bash
# -------------------------------------------------------------------
#  Daemon Hunter — Safe, self‑learning MIUI process suppressor
# -------------------------------------------------------------------
#  Usage:
#    ./daemonhunter.sh              # Run in foreground with live status
#    ./daemonhunter.sh --daemon     # Run in background (daemon mode)
#    ./daemonhunter.sh --verbose    # Foreground with full console logging
#    ./daemonhunter.sh --stop       # Stop any running daemon
#    ./daemonhunter.sh --status     # Show if daemon is running
# -------------------------------------------------------------------

# Fallback logging (if library missing)
db_log_tool_start() { :; }
db_log_tool_end() { :; }
db_log_action() { :; }

source lib/miuiserperuser_common.sh 2>/dev/null || true

# Paths
BRAIN="data/daemonhunter_brain.txt"
LOG="logs/daemonhunter.log"
SAFETY_LIST="data/miui_hyperos_bloat_list.conf"
PID_FILE="data/daemonhunter.pid"

mkdir -p data logs
touch "$BRAIN" "$LOG" 2>/dev/null

GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; CYAN='\033[0;36m'; BLUE='\033[0;34m'; NC='\033[0m'

# ------------------------------------------------------------
#  Shell dispatcher
# ------------------------------------------------------------
run_shell() {
    if [[ -x "$HOME/.shizuku/rish" ]]; then
        "$HOME/.shizuku/rish" -c "$*" 2>/dev/null
    elif command -v adb >/dev/null 2>&1; then
        adb shell "$@" 2>/dev/null
    else
        return 1
    fi
}

# ------------------------------------------------------------
#  Command line parsing
# ------------------------------------------------------------
DAEMON_MODE=0
VERBOSE=0
ACTION=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --daemon)   DAEMON_MODE=1 ;;
        --verbose)  VERBOSE=1 ;;
        --stop)     ACTION="stop" ;;
        --status)   ACTION="status" ;;
        --help|-h)  ACTION="help" ;;
        *) echo -e "${RED}Unknown option: $1${NC}"; ACTION="help" ;;
    esac
    shift
done

if [[ "$ACTION" == "help" ]]; then
    echo "Usage: $0 [--daemon] [--verbose] [--stop] [--status]"
    echo ""
    echo "  --daemon    Run in background (silent, logs to file)"
    echo "  --verbose   Show detailed console output (foreground)"
    echo "  --stop      Stop a running daemon instance"
    echo "  --status    Check if daemon is running"
    exit 0
fi

if [[ "$ACTION" == "stop" ]]; then
    if [[ -f "$PID_FILE" ]]; then
        pid=$(cat "$PID_FILE")
        if kill -0 "$pid" 2>/dev/null; then
            kill "$pid"
            echo -e "${GREEN}Daemon Hunter (PID $pid) stopped.${NC}"
        else
            echo -e "${YELLOW}PID file exists but process not running. Cleaning up.${NC}"
            rm -f "$PID_FILE"
        fi
    else
        echo -e "${CYAN}No running Daemon Hunter found.${NC}"
    fi
    exit 0
fi

if [[ "$ACTION" == "status" ]]; then
    if [[ -f "$PID_FILE" ]]; then
        pid=$(cat "$PID_FILE")
        if kill -0 "$pid" 2>/dev/null; then
            echo -e "${GREEN}Daemon Hunter is running (PID $pid).${NC}"
        else
            echo -e "${YELLOW}PID file exists but process not running.${NC}"
            rm -f "$PID_FILE"
        fi
    else
        echo -e "${CYAN}Daemon Hunter is not running.${NC}"
    fi
    exit 0
fi

# Prevent duplicate instances
if [[ -f "$PID_FILE" ]]; then
    pid=$(cat "$PID_FILE")
    if kill -0 "$pid" 2>/dev/null; then
        echo -e "${RED}Daemon Hunter is already running (PID $pid).${NC}"
        echo -e "Use '$0 --stop' to stop it first."
        exit 1
    else
        rm -f "$PID_FILE"
    fi
fi

# ------------------------------------------------------------
#  Initialization
# ------------------------------------------------------------
if ! run_shell "echo OK" &>/dev/null; then
    echo -e "${RED}[ERROR] Cannot connect to device. Ensure Shizuku or ADB is running.${NC}"
    exit 1
fi

declare -A targets

# Load from safety list
if [[ -f "$SAFETY_LIST" ]]; then
    while IFS='=' read -r category pkg; do
        [[ -z "$pkg" ]] && continue
        if [[ "$category" == "SAFE" || "$category" == "CULL" ]]; then
            targets["$pkg"]=1
        fi
    done < <(grep -E '^(SAFE|CULL)=' "$SAFETY_LIST" 2>/dev/null)
    echo "[$(date)] Loaded $(grep -cE '^(SAFE|CULL)=' "$SAFETY_LIST" 2>/dev/null || echo 0) targets from safety list" >> "$LOG"
fi

# Load from brain
if [[ -f "$BRAIN" ]]; then
    while IFS='|' read -r pkg _; do
        [[ -n "$pkg" ]] && targets["$pkg"]=1
    done < "$BRAIN"
    echo "[$(date)] Loaded $(wc -l < "$BRAIN") learned targets from brain" >> "$LOG"
fi

echo "[$(date)] Daemon Hunter started — monitoring ${#targets[@]} packages" >> "$LOG"

# ------------------------------------------------------------
#  Daemon mode: fork to background
# ------------------------------------------------------------
if [[ $DAEMON_MODE -eq 1 ]]; then
    echo -e "${GREEN}Starting Daemon Hunter in background...${NC}"
    echo -e "Logs: $LOG"
    echo -e "Stop with: $0 --stop"
    nohup "$0" --verbose >> "$LOG" 2>&1 &
    echo $! > "$PID_FILE"
    sleep 1
    if kill -0 "$(cat "$PID_FILE")" 2>/dev/null; then
        echo -e "${GREEN}Daemon started with PID $(cat "$PID_FILE")${NC}"
    else
        echo -e "${RED}Failed to start daemon. Check logs.${NC}"
        rm -f "$PID_FILE"
        exit 1
    fi
    exit 0
fi

# ------------------------------------------------------------
#  Foreground mode (with optional verbose output)
# ------------------------------------------------------------
cleanup() {
    echo -e "\n${YELLOW}[$(date)] Daemon Hunter stopping...${NC}"
    rm -f "$PID_FILE"
    db_log_tool_end "DaemonHunter" "0"
    exit 0
}
trap cleanup SIGINT SIGTERM

echo $$ > "$PID_FILE"

if [[ $VERBOSE -eq 0 ]]; then
    # Compact live status mode
    echo -e "${CYAN}Daemon Hunter running — Press Ctrl+C to stop${NC}"
    echo -e "Targets: ${#targets[@]} packages | Logs: $LOG"
    echo ""
    round=0
    while true; do
        ((round++))
        action_count=0
        last_pkg=""
        for pkg in "${!targets[@]}"; do
            if run_shell "pm list packages" 2>/dev/null | grep -q "$pkg"; then
                run_shell "am force-stop $pkg" 2>/dev/null
                for op in 27 26 4 6 14 1 60; do
                    run_shell "cmd appops set $pkg $op ignore" 2>/dev/null
                done
                ((action_count++))
                last_pkg="$pkg"
                # Update status line
                printf "\r${GREEN}[Round %-3d]${NC} Actions: %-3d  Last: %-40s" "$round" "$action_count" "${last_pkg:0:40}"
            fi
            sleep 0.3
        done
        # Log round completion
        echo "[$(date)] Round $round complete — $action_count actions taken" >> "$LOG"
        printf "\r${GREEN}[Round %-3d]${NC} Actions: %-3d  Last: %-40s ${YELLOW}(sleeping 40s)${NC}" "$round" "$action_count" "${last_pkg:0:40}"
        sleep 40
    done
else
    # Verbose mode: log everything to console
    echo -e "${CYAN}Daemon Hunter — Verbose Mode${NC}"
    echo -e "Logs also written to: $LOG"
    echo ""
    round=0
    while true; do
        ((round++))
        echo -e "${BLUE}═══ Round $round started ═══${NC}"
        action_count=0
        for pkg in "${!targets[@]}"; do
            if run_shell "pm list packages" 2>/dev/null | grep -q "$pkg"; then
                run_shell "am force-stop $pkg" 2>/dev/null
                for op in 27 26 4 6 14 1 60; do
                    run_shell "cmd appops set $pkg $op ignore" 2>/dev/null
                done
                ((action_count++))
                echo -e "  ${GREEN}✓${NC} $pkg"
                # Log to file as well
                echo "[$(date)] Force-stopped → $pkg" >> "$LOG"
                # Learn if new
                if ! grep -q "^$pkg|" "$BRAIN" 2>/dev/null; then
                    echo "$pkg|force-stop|$(date +%s)" >> "$BRAIN"
                    echo -e "  ${YELLOW}📘 Learned new target${NC}"
                fi
            fi
            sleep 0.5
        done
        echo -e "${CYAN}Round $round complete — $action_count actions taken${NC}"
        echo -e "${YELLOW}Sleeping 40 seconds...${NC}\n"
        sleep 40
    done
fi
