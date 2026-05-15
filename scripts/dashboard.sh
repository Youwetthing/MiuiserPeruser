#!/data/data/com.termux/files/usr/bin/bash
# -------------------------------------------------------------------
#  MiuiserPeruser Dashboard — Unified Command Center
# -------------------------------------------------------------------
#  Monitor daemons, run scans, access tools, view logs, and more.
#  Web UI: http://127.0.0.1:5000 (when Python dashboard is running)
# -------------------------------------------------------------------

GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'
BLUE='\033[0;34m'; CYAN='\033[0;36m'; MAGENTA='\033[0;35m'
WHITE='\033[1;37m'; BOLD='\033[1m'; NC='\033[0m'

# Paths
LIB="lib/miuiserperuser_common.sh"
PERUSE_CONTROL="tools/peruse_control.sh"
DAEMON_HUNTER="bin/daemonhunter.sh"
RAM_SLAMMER="bin/ram_slammer.sh"
BATTERY_TRUTH="scripts/battery_truth.sh"
HIDDEN_SETTINGS="tools/hidden_settings.sh"
COMPLETION_MARKER="data/.peruse_completed"
PID_DIR="pipes"
LOG_DIR="logs"
DATA_DIR="data"

# Source common functions if available
if [[ -f "$LIB" ]]; then
    source "$LIB"
else
    db_log_action() { :; }
    db_log_tool_start() { :; }
    db_log_tool_end() { :; }
fi

# ------------------------------------------------------------
#  Helper: run shell command via Shizuku/ADB
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
#  Connection status
# ------------------------------------------------------------
check_connection() {
    if run_shell "echo OK" &>/dev/null; then
        echo -e "${GREEN}● Connected${NC} (Shizuku/ADB)"
        return 0
    else
        echo -e "${RED}○ Disconnected${NC} — Shizuku or ADB required"
        return 1
    fi
}

# ------------------------------------------------------------
#  Daemon status helpers (placeholders — replace with your puns)
# ------------------------------------------------------------
DAEMONS=(
    "bebopd:bebopd:bebopd"
    "daemonhunter:daemonhunter:daemonhunter"
    "fugitoidd:fugitoidd:fugitoidd"
    "krangd:krangd:krangd"
    "leatherheadd:leatherheadd:leatherheadd"
    "metalhead:metalhead:metalhead"
    "networkd:networkd:networkd"
    "rahzerd:rahzerd:rahzerd"
    "ratkingd:ratkingd:ratkingd"
    "rocksteadyd:rocksteadyd:rocksteadyd"
    "shredderd:shredderd:shredderd"
    "splinterd:splinterd:splinterd"
    "tigerclawd:tigerclawd:tigerclawd"
    "turtlecomd:turtlecomd:turtlecomd"
)

daemon_running() {
    local pid_file="${PID_DIR}/$1.pid"
    local proc_name="$2"
    if [[ -f "$pid_file" ]]; then
        local pid=$(cat "$pid_file" 2>/dev/null)
        if kill -0 "$pid" 2>/dev/null; then
            return 0
        fi
    fi
    pgrep -f "$proc_name" >/dev/null 2>&1
}

daemon_status_icon() {
    if daemon_running "$1" "$2"; then
        echo -e "${GREEN}●${NC}"
    else
        echo -e "${RED}○${NC}"
    fi
}

# ------------------------------------------------------------
#  Display header with daemon status bar
# ------------------------------------------------------------
display_header() {
    clear
    term_width=$(tput cols 2>/dev/null || echo 80)
    echo -e "${CYAN}$(printf '=%.0s' $(seq 1 $term_width))${NC}"
    echo -e "${CYAN}   🐢 MIUISERPERUSER DASHBOARD${NC}"
    echo -e "${CYAN}$(printf '=%.0s' $(seq 1 $term_width))${NC}"
    echo ""
    
    # Connection and device info
    echo -ne "Device: "
    check_connection
    if run_shell "echo OK" &>/dev/null; then
        local model=$(run_shell "getprop ro.product.model" 2>/dev/null | tr -d '\r\n')
        local miui=$(run_shell "getprop ro.miui.ui.version.name" 2>/dev/null | tr -d '\r\n')
        local android=$(run_shell "getprop ro.build.version.release" 2>/dev/null | tr -d '\r\n')
        echo -e "Model: ${model:-Unknown}  |  MIUI: ${miui:-N/A}  |  Android: ${android:-N/A}"
    fi
    
    # Training status
    if [[ -f "$COMPLETION_MARKER" ]]; then
        echo -e "📖 Training: ${GREEN}COMPLETED${NC} — RAM Slammer unlocked"
    else
        echo -e "📖 Training: ${YELLOW}INCOMPLETE${NC} — run Peruse Control first"
    fi
    echo ""
    
    # Daemon status bar
    echo -e "${MAGENTA}━━━ DAEMON STATUS ━━━${NC}"
    local count=0
    for entry in "${DAEMONS[@]}"; do
        IFS=':' read -r display pid_base proc <<< "$entry"
        printf "  %-15s %s" "$display" "$(daemon_status_icon "$pid_base" "$proc")"
        ((count++))
        if (( count % 4 == 0 )); then echo ""; fi
    done
    [[ $(( ${#DAEMONS[@]} % 4 )) -ne 0 ]] && echo ""
    echo ""
}

# ------------------------------------------------------------
#  Main menu
# ------------------------------------------------------------
main_menu() {
    echo -e "${CYAN}━━━ MAIN MENU ━━━${NC}"
    echo ""
    echo "  ${GREEN}1${NC}) 🔍 Full Anomaly Check"
    echo "  ${GREEN}2${NC}) 🛡️ System Scan"
    echo "  ${GREEN}3${NC}) 🧰 Toolkit"
    echo "  ${GREEN}4${NC}) 📋 Logging & Reports"
    echo "  ${GREEN}5${NC}) 👁️ Live View"
    echo "  ${GREEN}6${NC}) ⚙️ System Actions"
    echo "  ${GREEN}7${NC}) 🚪 Exit"
    echo ""
    echo -e "${CYAN}─────────────────────────────────────────${NC}"
    echo -e "🌐 Web Dashboard: ${WHITE}http://127.0.0.1:5000${NC} (if running)"
    echo ""
    echo -n "Choice: "
}

# ------------------------------------------------------------
#  System Scan submenu
# ------------------------------------------------------------
system_scan_menu() {
    while true; do
        clear
        display_header
        echo -e "${CYAN}━━━ SYSTEM SCAN ━━━${NC}"
        echo ""
        echo "  ${GREEN}1${NC}) 👾 Daemon Management"
        echo "  ${GREEN}2${NC}) 🔎 Run Full Scan"
        echo "  ${GREEN}3${NC}) 📊 View Scan Results"
        echo "  ${YELLOW}b${NC}) Back to main"
        echo ""
        echo -n "Choice: "
        read -r choice
        case $choice in
            1) daemon_management ;;
            2) run_full_scan ;;
            3) view_scan_results ;;
            b|B) break ;;
            *) echo "Invalid"; sleep 1 ;;
        esac
    done
}

# ------------------------------------------------------------
#  Daemon Management
# ------------------------------------------------------------
daemon_management() {
    clear
    display_header
    echo -e "${CYAN}━━━ DAEMON MANAGEMENT ━━━${NC}"
    echo ""
    echo "  ${GREEN}1${NC}) Start All Core Daemons"
    echo "  ${GREEN}2${NC}) Stop All Core Daemons"
    echo "  ${GREEN}3${NC}) Start Daemon Hunter (background)"
    echo "  ${GREEN}4${NC}) Stop Daemon Hunter"
    echo "  ${YELLOW}b${NC}) Back"
    echo ""
    echo -n "Choice: "
    read -r choice
    case $choice in
        1) ./launch_core_daemon.sh ;;
        2) pkill -f "bebopd|fugitoidd|krangd|leatherheadd|metalhead|networkd|rahzerd|ratkingd|rocksteadyd|shredderd|splinterd|tigerclawd|turtlecomd"; echo "All core daemons stopped."; sleep 1 ;;
        3) ./bin/daemonhunter.sh --daemon; sleep 1 ;;
        4) ./bin/daemonhunter.sh --stop; sleep 1 ;;
        b|B) return ;;
        *) echo "Invalid"; sleep 1 ;;
    esac
}

run_full_scan() {
    clear
    display_header
    echo -e "${YELLOW}Running full system scan...${NC}"
    if [[ -x "bin/superhero" ]]; then
        ./bin/superhero --scan
    elif [[ -x "bin/splinterd" ]]; then
        ./bin/splinterd --scan
    else
        echo "Scan tool not available."
    fi
    read -p "Press Enter..."
}

view_scan_results() {
    if [[ -f "$LOG_DIR/superhero_scan.log" ]]; then
        less "$LOG_DIR/superhero_scan.log"
    else
        echo "No scan results found."
        read -p "Press Enter..."
    fi
}

# ------------------------------------------------------------
#  Full Anomaly Check (bin/superhero)
# ------------------------------------------------------------
full_anomaly_check() {
    clear
    display_header
    echo -e "${YELLOW}Launching Superhero — Full Anomaly Check...${NC}"
    if [[ -x "bin/superhero" ]]; then
        ./bin/superhero
    else
        echo -e "${RED}bin/superhero not found or not executable.${NC}"
    fi
    read -p "Press Enter to return..."
}

# ------------------------------------------------------------
#  Toolkit (only working tools)
# ------------------------------------------------------------
toolkit_menu() {
    while true; do
        clear
        display_header
        echo -e "${CYAN}━━━ TOOLKIT ━━━${NC}"
        echo ""
        echo "  ${GREEN}1${NC}) 📖 Peruse Control (Interactive Teacher)"
        echo "  ${GREEN}2${NC}) 💥 RAM Slammer"
        echo "  ${GREEN}3${NC}) 🔋 Battery Truth"
        echo "  ${GREEN}4${NC}) 📡 ADB Commands"
        [[ -x "tools/build_uid_map.sh" ]] && echo "  ${GREEN}5${NC}) 🧠 Build UID Map"
        echo "  ${YELLOW}b${NC}) Back"
        echo ""
        echo -n "Choice: "
        read -r choice
        case $choice in
            1) [[ -x "$PERUSE_CONTROL" ]] && "$PERUSE_CONTROL" || echo "Not found."; sleep 1 ;;
            2) 
                if [[ -f "$COMPLETION_MARKER" ]]; then
                    [[ -x "$RAM_SLAMMER" ]] && "$RAM_SLAMMER" || echo "RAM Slammer not available."
                else
                    echo -e "${RED}Complete Peruse Control first!${NC}"
                    sleep 1
                fi
                ;;
            3) [[ -x "$BATTERY_TRUTH" ]] && "$BATTERY_TRUTH" || echo "Not found."; read -p "Press Enter..." ;;
            4) [[ -x "$HIDDEN_SETTINGS" ]] && "$HIDDEN_SETTINGS" || echo "Not found."; read -p "Press Enter..." ;;
            5) [[ -x "tools/build_uid_map.sh" ]] && ./tools/build_uid_map.sh; read -p "Press Enter..." ;;
            b|B) break ;;
            *) echo "Invalid"; sleep 1 ;;
        esac
    done
}

# ------------------------------------------------------------
#  Logging & Reports
# ------------------------------------------------------------
logging_menu() {
    while true; do
        clear
        display_header
        echo -e "${CYAN}━━━ LOGGING & REPORTS ━━━${NC}"
        echo ""
        echo "  ${GREEN}1${NC}) View Latest Log (select from list)"
        echo "  ${GREEN}2${NC}) Export All Logs (tar.gz)"
        echo "  ${GREEN}3${NC}) Clear Old Logs (>7 days)"
        echo "  ${YELLOW}b${NC}) Back"
        echo ""
        echo -n "Choice: "
        read -r choice
        case $choice in
            1)
                logs=($(ls -1t "$LOG_DIR"/*.log 2>/dev/null | head -10))
                if [[ ${#logs[@]} -eq 0 ]]; then
                    echo "No logs found."
                    read -p "Press Enter..."
                else
                    echo "Recent logs:"
                    for i in "${!logs[@]}"; do
                        printf "  %2d) %s\n" $((i+1)) "$(basename "${logs[$i]}")"
                    done
                    read -p "Select number (or Enter to cancel): " log_choice
                    if [[ "$log_choice" =~ ^[0-9]+$ ]] && (( log_choice > 0 && log_choice <= ${#logs[@]} )); then
                        less "${logs[$((log_choice-1))]}"
                    fi
                fi
                ;;
            2)
                mkdir -p exports
                tar -czf "exports/logs_$(date +%Y%m%d_%H%M%S).tar.gz" "$LOG_DIR"/*.log 2>/dev/null
                echo "Logs exported to exports/"
                read -p "Press Enter..."
                ;;
            3)
                find "$LOG_DIR" -name "*.log" -mtime +7 -delete
                echo "Old logs removed."
                sleep 1
                ;;
            b|B) break ;;
            *) echo "Invalid"; sleep 1 ;;
        esac
    done
}

# ------------------------------------------------------------
#  Live View
# ------------------------------------------------------------
live_view() {
    clear
    display_header
    echo -e "${CYAN}━━━ LIVE VIEW ━━━${NC}"
    echo ""
    echo "  ${GREEN}1${NC}) Watch Daemon Hunter log"
    echo "  ${GREEN}2${NC}) Watch Superhero scan log"
    echo "  ${GREEN}3${NC}) Live process monitor (top)"
    echo "  ${YELLOW}b${NC}) Back"
    echo ""
    echo -n "Choice: "
    read -r choice
    case $choice in
        1) tail -f "$LOG_DIR/daemonhunter.log" 2>/dev/null ;;
        2) tail -f "$LOG_DIR/superhero_scan.log" 2>/dev/null ;;
        3) run_shell "top -n 1" | head -20; read -p "Press Enter..." ;;
        b|B) return ;;
        *) echo "Invalid"; sleep 1 ;;
    esac
}

# ------------------------------------------------------------
#  System Actions
# ------------------------------------------------------------
system_actions() {
    while true; do
        clear
        display_header
        echo -e "${CYAN}━━━ SYSTEM ACTIONS ━━━${NC}"
        echo ""
        echo "  ${GREEN}1${NC}) 🔄 Refresh Dashboard"
        echo "  ${GREEN}2${NC}) 🧹 Clean old logs"
        echo "  ${GREEN}3${NC}) 📦 Backup configs"
        echo "  ${GREEN}4${NC}) 🔌 Reconnect ADB"
        echo "  ${GREEN}5${NC}) 🐚 Launch ADB Shell"
        echo "  ${GREEN}6${NC}) 🌐 Start Python Web Dashboard"
        echo "  ${YELLOW}b${NC}) Back"
        echo ""
        echo -n "Choice: "
        read -r choice
        case $choice in
            1) return ;;
            2) find "$LOG_DIR" -name "*.log" -mtime +7 -delete; echo "Old logs cleaned."; sleep 1 ;;
            3) mkdir -p backups; tar -czf "backups/backup_$(date +%Y%m%d_%H%M%S).tar.gz" data/*.conf data/*.db 2>/dev/null; echo "Backup created."; sleep 1 ;;
            4) adb reconnect; read -p "Press Enter..." ;;
            5) adb shell ;;
            6) 
                if pgrep -f "syndicate_app.py" >/dev/null; then
                    echo "Web dashboard already running."
                else
                    python syndicate_app.py &
                    echo "Started Python dashboard on http://127.0.0.1:5000"
                fi
                sleep 1
                ;;
            b|B) break ;;
            *) echo "Invalid"; sleep 1 ;;
        esac
    done
}

# ------------------------------------------------------------
#  Main loop
# ------------------------------------------------------------
db_log_tool_start "Dashboard"

while true; do
    clear
    display_header
    main_menu
    read -r choice
    case $choice in
        1) full_anomaly_check ;;
        2) system_scan_menu ;;
        3) toolkit_menu ;;
        4) logging_menu ;;
        5) live_view ;;
        6) system_actions ;;
        7) break ;;
        *) echo "Invalid choice"; sleep 1 ;;
    esac
done

db_log_tool_end "Dashboard" "0"
echo -e "\n${CYAN}Dashboard closed.${NC}"
