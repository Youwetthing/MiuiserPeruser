#!/usr/bin/env bash
source "$(dirname "$0")/lib/tool_backend.sh"
# ==============================================================================
# StalkerSlayer v9.1 - Xiaomi Telemetry Blocker (Clean, Working)
# ==============================================================================

set -uo pipefail
IFS=$'\n\t'

# --- Configuration ---
readonly BASE_DIR
BASE_DIR="${HOME}/MiuiserPeruser"
readonly LOG_DIR
LOG_DIR="${BASE_DIR}/log_cabin/telemetry"
readonly LOCK_DIR
LOCK_DIR="${HOME}/.StalkerSlayer.lock"
readonly RISH_PATH
RISH_PATH="${HOME}/Rish/rish"
readonly KILLER_PID_FILE
KILLER_PID_FILE="${BASE_DIR}/.killer.pid"

# Colors (using tput for reliability)
if [[ -t 1 ]]; then
    readonly BOLD
    BOLD=$(tput bold 2>/dev/null || echo '')
    readonly GREEN
    GREEN=$(tput setaf 2 2>/dev/null || echo '')
    readonly RED
    RED=$(tput setaf 1 2>/dev/null || echo '')
    readonly YELLOW
    YELLOW=$(tput setaf 3 2>/dev/null || echo '')
    readonly BLUE
    BLUE=$(tput setaf 4 2>/dev/null || echo '')
    readonly CYAN
    CYAN=$(tput setaf 6 2>/dev/null || echo '')
    readonly RESET
    RESET=$(tput sgr0 2>/dev/null || echo '')
else
    readonly BOLD=''; readonly GREEN=''; readonly RED=''
    readonly YELLOW=''; readonly BLUE=''; readonly CYAN=''; readonly RESET=''
fi

# Telemetry packages (only those that are actually problematic)
readonly PACKAGES=(
    "com.miui.analytics"
    "com.xiaomi.joyose"
    "com.miui.daemon"
    "com.miui.cloudservice"
    "com.miui.hybrid"
    "com.miui.bugreport"
    "com.miui.systemAdSolution"
    "com.xiaomi.adfeedback"
    "com.miui.msa"
    "com.miui.msa.global"
    "com.miui.cloudbackup"
    "com.xiaomi.discover"
)

# --- Helpers ---
exec_priv() {
    local cmd
    cmd="$1"
    if [[ -x "$RISH_PATH" ]]; then
        "$RISH_PATH" -c "$cmd" 2>/dev/null
    elif command -v adb &>/dev/null && adb devices | grep -q 'device$'; then
        adb shell "$cmd" 2>/dev/null
    else
        return 1
    fi
}

check_shell() {
    if [[ -x "$RISH_PATH" ]]; then echo "rish"; return 0
    elif command -v adb &>/dev/null && adb devices | grep -q 'device$'; then echo "adb"; return 0
    else echo "none"; return 1; fi
}

# --- UI ---
print_header() {
    echo -e "\n${CYAN}${BOLD}StalkerSlayer v9.1 - Telemetry Blocker${RESET}\n"
}

print_success() { echo -e "${GREEN}✓ $1${RESET}"; }
print_error()   { echo -e "${RED}✗ $1${RESET}" >&2; }
print_warning() { echo -e "${YELLOW}⚠ $1${RESET}"; }
print_info()    { echo -e "${BLUE}ℹ $1${RESET}"; }

pause() {
    echo ""
    read -r -n 1 -s -p "Press any key to continue..."
    echo ""
}

confirm() {
    local answer
    read -r -p "$1 [y/N]: " answer
    [[ "$answer" =~ ^[Yy]$ ]]
}

# --- Core Actions ---
kill_all() {
    for pkg in "${PACKAGES[@]}"; do
        exec_priv "am force-stop $pkg" >/dev/null
        exec_priv "killall -9 $pkg" 2>/dev/null
    done
}

restrict_all() {
    for pkg in "${PACKAGES[@]}"; do
        exec_priv "appops set $pkg RUN_IN_BACKGROUND ignore" >/dev/null
        exec_priv "appops set $pkg WAKE_LOCK ignore" >/dev/null
        exec_priv "appops set $pkg START_FOREGROUND ignore" >/dev/null
    done
}

clear_data() {
    for pkg in "${PACKAGES[@]}"; do
        exec_priv "pm clear $pkg" >/dev/null
    done
    exec_priv "rm -rf /data/system/analytics/*" >/dev/null
}

# --- Background Killer ---
start_killer() {
    if [[ -f "$KILLER_PID_FILE" ]] && kill -0 "$(cat "$KILLER_PID_FILE")" 2>/dev/null; then
        print_warning "Killer already running (PID $(cat "$KILLER_PID_FILE"))"
        return
    fi
    print_info "Starting background killer (kills telemetry every 10 seconds)"
    # Create a loop that runs in background via rish
    nohup sh -c "while true; do for pkg in ${PACKAGES[*]}; do am force-stop \$pkg 2>/dev/null; done; sleep 10; done" >/dev/null 2>&1 &
    local pid=$!
    echo "$pid" > "$KILLER_PID_FILE"
    print_success "Killer started (PID $pid)"
}

stop_killer() {
    if [[ -f "$KILLER_PID_FILE" ]]; then
        local pid
        pid=$(cat "$KILLER_PID_FILE")
        if kill -0 "$pid" 2>/dev/null; then
            kill "$pid"
            print_success "Stopped killer (PID $pid)"
        else
            print_warning "Killer not running"
        fi
        rm -f "$KILLER_PID_FILE"
    else
        print_warning "No killer PID file"
    fi
}

# --- Status (clean, no broken output) ---
run_status() {
    clear
    print_header
    
    local shell_type
    shell_type=$(check_shell)
    if [[ "$shell_type" == "none" ]]; then
        print_error "No rish or adb available"
        pause
        return
    fi
    print_info "Using $shell_type"
    
    # Show running processes
    echo -e "\n${BOLD}Running telemetry processes:${RESET}"
    local running=0
    for pkg in "${PACKAGES[@]}"; do
        local pid
        pid=$(exec_priv "pidof $pkg" | awk '{print $1}')
        if [[ -n "$pid" ]]; then
            echo "  ${YELLOW}▶${RESET} $pkg (PID $pid)"
            ((running++))
        fi
    done
    if [[ $running -eq 0 ]]; then
        print_success "No telemetry processes running"
    fi
    
    # Show AppOps restrictions (simple summary)
    echo -e "\n${BOLD}AppOps restrictions:${RESET}"
    local all_good=0
    for pkg in "${PACKAGES[@]}"; do
        local bg
        bg=$(exec_priv "appops get $pkg RUN_IN_BACKGROUND" | grep -o 'ignore\|allow\|default' | head -1)
        local wl
        wl=$(exec_priv "appops get $pkg WAKE_LOCK" | grep -o 'ignore\|allow\|default' | head -1)
        local fg
        fg=$(exec_priv "appops get $pkg START_FOREGROUND" | grep -o 'ignore\|allow\|default' | head -1)
        [[ -z "$bg" ]] && bg="default"
        [[ -z "$wl" ]] && wl="default"
        [[ -z "$fg" ]] && fg="default"
        if [[ "$bg" == "ignore" && "$wl" == "ignore" && "$fg" == "ignore" ]]; then
            echo "  ${GREEN}✓${RESET} $pkg (BG=$bg, WL=$wl, FG=$fg)"
            ((all_good++))
        else
            echo "  ${RED}○${RESET} $pkg (BG=$bg, WL=$wl, FG=$fg) – run Slay to fix"
        fi
    done
    
    if [[ $all_good -eq ${#PACKAGES[@]} ]]; then
        print_success "All packages properly restricted"
    fi
    
    pause
}

# --- Scan ---
run_scan() {
    show_mikey_findings
    clear
    print_header
    
    local shell_type
    shell_type=$(check_shell)
    if [[ "$shell_type" == "none" ]]; then
        print_error "No rish or adb available"
        pause
        return
    fi
    
    mkdir -p "$LOG_DIR"
    local report
    report="${LOG_DIR}/scan_$(date +%Y%m%d_%H%M%S).txt"
    echo "=== SCAN REPORT $(date) ===" > "$report"
    
    echo -e "${BOLD}Active telemetry processes:${RESET}"
    local active=0
    for pkg in "${PACKAGES[@]}"; do
        local pid
        pid=$(exec_priv "pidof $pkg" | awk '{print $1}')
        if [[ -n "$pid" ]]; then
            echo "  ${YELLOW}▶${RESET} $pkg (PID $pid)"
            echo "PROCESS: $pkg PID=$pid" >> "$report"
            ((active++))
        fi
    done
    if [[ $active -eq 0 ]]; then
        print_success "No active processes found"
    else
        print_warning "$active process(es) running"
    fi
    
    echo -e "\n${BOLD}Report saved:${RESET} $report"
    pause
}

# --- Slay (one-time kill + restrict + clear) ---
run_slay() {
    clear
    print_header
    
    local shell_type
    shell_type=$(check_shell)
    if [[ "$shell_type" == "none" ]]; then
        print_error "No rish or adb available"
        pause
        return
    fi
    
    echo -e "${YELLOW}${BOLD}⚠ This will force-stop, restrict, and clear all telemetry packages.${RESET}"
    if ! confirm "Continue?"; then
        print_info "Aborted"
        pause
        return
    fi
    
    echo -n "Killing processes... "
    kill_all
    print_success "done"
    
    echo -n "Applying AppOps restrictions... "
    restrict_all
    print_success "done"
    
    echo -n "Clearing app data and caches... "
    clear_data
    print_success "done"
    
    echo ""
    print_success "One-time neutralization complete"
    echo ""
    
    if confirm "Start background killer to keep them dead?"; then
        start_killer
    fi
    
    pause
}

# --- Restore (stop killer + optional reset) ---
run_restore() {
    clear
    print_header
    
    stop_killer
    
    echo ""
    print_info "AppOps restrictions can be reset manually with:"
    echo "  for pkg in ${PACKAGES[*]}; do"
    echo "    appops set \$pkg RUN_IN_BACKGROUND allow"
    echo "    appops set \$pkg WAKE_LOCK allow"
    echo "    appops set \$pkg START_FOREGROUND allow"
    echo "  done"
    echo ""
    pause
}

# --- Help ---
show_help() {
    clear
    print_header
    echo -e "${BOLD}Usage:${RESET}"
    echo "  ./StalkerSlayer.sh          - Interactive menu"
    echo "  ./StalkerSlayer.sh --status - Show running processes + AppOps"
    echo "  ./StalkerSlayer.sh --scan   - Generate scan report"
    echo "  ./StalkerSlayer.sh --slay   - One-time kill, restrict, clear"
    echo "  ./StalkerSlayer.sh --start-killer - Start background killer"
    echo "  ./StalkerSlayer.sh --stop-killer  - Stop background killer"
    echo "  ./StalkerSlayer.sh --help   - This help"
    echo ""
    echo -e "${BOLD}What this script does:${RESET}"
    echo "  • Force-stops telemetry apps (am force-stop)"
    echo "  • Blocks background execution, wake locks, foreground start (appops)"
    echo "  • Clears app data and system analytics cache"
    echo "  • Optional persistent killer to keep them dead"
    echo ""
    echo -e "${BOLD}Limitations (no root):${RESET}"
    echo "  • Cannot permanently disable packages (pm disable-user fails)"
    echo "  • Cannot block network-level connections"
    echo "  • Background killer only kills, doesn't prevent respawning"
    echo "  • AppOps 'ignore' is the strongest restriction available"
    pause
}

# --- Menu ---

# --- Mikey findings from last superhero scan ---
show_mikey_findings() {
    local json
    json="${HOME}/MiuiserPeruser/data/last_scan.json"
    echo -e "\n${BOLD}${CYAN}=== MIKEY Telemetry Findings ===${RESET}"
    if [ ! -f "$json" ]; then
        echo -e "${YELLOW}  No scan data. Run: superhero --standard${RESET}"
        return
    fi
    python3 << 'PY'
import json, os
path = os.path.expanduser("~/MiuiserPeruser/data/last_scan.json")
try:
    data = json.load(open(path))
    mikey = [f for f in data if f.get("turtle") == "MIKEY"]
    if not mikey:
        print("  No MIKEY findings.")
    else:
        actionable = {"MIUI_ANALYTICS","MSA_ACTIVE","MIUI_DAEMON","GDPR_OPT_OUT"}
        for f in mikey:
            pri = f.get("priority","?")
            typ = f.get("type","?")
            desc = f.get("description","")
            icons = {"CRITICAL":"[!!]","HIGH":"[HI]","MEDIUM":"[MD]","LOW":"[LO]"}
            act = "[ACTIONABLE]" if typ in actionable else "[INFO]"
            print(f"  {icons.get(pri,'[?]')} [{pri}] {typ} {act}")
            print(f"      {desc}")
            print()
except Exception as e:
    print(f"  Error: {e}")
PY
}

slay_mikey_findings() {
    local json
    json="${HOME}/MiuiserPeruser/data/last_scan.json"
    echo -e "\n${BOLD}${RED}=== Slaying MIKEY Findings ===${RESET}"
    if [ ! -f "$json" ]; then
        echo -e "${YELLOW}  No scan data.${RESET}"; return
    fi

    local findings
    findings=$(python3 << 'PY'
import json, os
data = json.load(open(os.path.expanduser("~/MiuiserPeruser/data/last_scan.json")))
for f in data:
    if f.get("turtle") == "MIKEY":
        print(f.get("type",""))
PY
)

    echo -e "${YELLOW}  Actions to execute:${RESET}"
    echo "$findings" | while read -r typ; do
        case "$typ" in
            MIUI_ANALYTICS) echo "    - Restrict MIUI Analytics" ;;
            MSA_ACTIVE)     echo "    - Force-stop MSA" ;;
            MIUI_DAEMON)    echo "    - Force-stop MiuiDaemon" ;;
            GDPR_OPT_OUT)   echo "    - Set GDPR opt-out prop" ;;
            *)              echo "    - $typ (info only)" ;;
        esac
    done

    printf "${CYAN}  Proceed? [y/N]: ${RESET}"
    read -r confirm
    [[ "$confirm" != "y" && "$confirm" != "Y" ]] && { echo "Cancelled."; return; }

    echo "$findings" | while read -r typ; do
        case "$typ" in
            MIUI_ANALYTICS)
                echo -e "  ${YELLOW}-> Restricting Analytics...${RESET}"
                exec_priv "am force-stop com.miui.analytics"
                exec_priv "cmd appops set com.miui.analytics RUN_IN_BACKGROUND ignore" ;;
            MSA_ACTIVE)
                echo -e "  ${YELLOW}-> Stopping MSA...${RESET}"
                exec_priv "am force-stop com.miui.msa.global" ;;
            MIUI_DAEMON)
                echo -e "  ${YELLOW}-> Stopping Daemon...${RESET}"
                exec_priv "am force-stop com.miui.daemon" ;;
            GDPR_OPT_OUT)
                echo -e "  ${YELLOW}-> Setting GDPR prop...${RESET}"
                exec_priv "setprop persist.sys.miui.gdpr 1" ;;
            MILLET_ACTIVE|PARTNER_TOKEN|SNO_TRACKING|FB_PARTNER_ID)
                echo -e "  ${CYAN}  $typ — no runtime fix available${RESET}" ;;
        esac
    done
    echo -e "\n${GREEN}Done. Run scan again to verify.${RESET}"
}


slay_mikey_findings() {
    local json
    json="${HOME}/MiuiserPeruser/data/last_scan.json"
    echo -e "\n${BOLD}${RED}=== Slaying MIKEY Findings ===${RESET}"
    if [ ! -f "$json" ]; then
        echo -e "${YELLOW}  No scan data.${RESET}"; return
    fi

    # Map finding types to actions
    local types
    types=$(python3 << 'PY'
import json, os
data = json.load(open(os.path.expanduser('~/MiuiserPeruser/data/last_scan.json')))
for f in data:
    if f.get('turtle') == 'MIKEY':
        print(f.get('type',''))
PY
)

    echo "$types" | while read -r typ; do
        case "$typ" in
            MIUI_ANALYTICS)
                echo -e "  ${YELLOW}Restricting MIUI Analytics...${RESET}"
                exec_priv "am force-stop com.miui.analytics"
                exec_priv "appops set com.miui.analytics RUN_IN_BACKGROUND ignore"
                ;;
            MSA_ACTIVE)
                echo -e "  ${YELLOW}Force-stopping MSA...${RESET}"
                exec_priv "am force-stop com.miui.msa.global"
                ;;
            MIUI_DAEMON)
                echo -e "  ${YELLOW}Force-stopping MiuiDaemon...${RESET}"
                exec_priv "am force-stop com.miui.daemon"
                ;;
            GDPR_OPT_OUT)
                echo -e "  ${YELLOW}Setting GDPR opt-out prop...${RESET}"
                exec_priv "setprop persist.sys.miui.gdpr 1"
                ;;
            MILLET_ACTIVE|PARTNER_TOKEN|SNO_TRACKING|FB_PARTNER_ID)
                echo -e "  ${CYAN}  $typ — informational only, no runtime fix available${RESET}"
                ;;
        esac
    done
    echo -e "\n${GREEN}Done. Run scan again to verify.${RESET}"
}

show_menu() {
    clear
    print_header
    echo -e "${BOLD}Main Menu${RESET}\n"
    echo "  1) Status"
    echo "  2) Scan"
    echo "  3) Slay (one-time kill + restrict + clear)"
    echo "  4) Start background killer"
    echo "  5) Stop background killer"
    echo "  6) Restore (stop killer + reset instructions)"
    echo "  7) Help"
    echo "  8) Mikey findings (display)"
    echo "  9) Slay Mikey findings"
    echo ""
    echo " 10) Exit  [or q]"
  echo " 11) DNS blocking (AdGuard)"
    echo -n "Choose [1-10]: "
}

# --- DNS Blocking ---
ADB="/data/data/com.termux/files/home/.cargo/bin/adb_cli"

dns_status() {
    local mode
    mode=$($ADB tcp 127.0.0.1:5555 shell "settings get global private_dns_mode" 2>/dev/null | tr -d '\r')
    local host
    host=$($ADB tcp 127.0.0.1:5555 shell "settings get global private_dns_specifier" 2>/dev/null | tr -d '\r')
    echo "  DNS mode: $mode"
    echo "  DNS host: $host"
    if echo "$host" | grep -q "adguard"; then
        echo "  Status: BLOCKING ACTIVE (AdGuard DNS)"
    else
        echo "  Status: NOT BLOCKING"
    fi
}

dns_block_enable() {
    echo "  Enabling AdGuard DNS blocking..."
    $ADB tcp 127.0.0.1:5555 shell "settings put global private_dns_mode hostname" 2>/dev/null
    $ADB tcp 127.0.0.1:5555 shell "settings put global private_dns_specifier dns.adguard.com" 2>/dev/null
    echo "  Blocking domains include:"
    echo "    analytics.miui.com"
    echo "    data.mistat.xiaomi.com"
    echo "    sdkconfig.ad.xiaomi.com"
    echo "    msg.global.xiaomi.net"
    echo "    tracking.miui.com"
    echo "  Done — AdGuard DNS active"
}

dns_block_disable() {
    echo "  Disabling DNS blocking..."
    $ADB tcp 127.0.0.1:5555 shell "settings put global private_dns_mode opportunistic" 2>/dev/null
    $ADB tcp 127.0.0.1:5555 shell "settings delete global private_dns_specifier" 2>/dev/null
    echo "  Done — DNS restored to automatic"
}

run_dns_menu() {
    echo ""
    echo "  DNS BLOCKING"
    echo "  ────────────────────────────────"
    dns_status
    echo ""
    echo "  [1] Enable AdGuard DNS blocking"
    echo "  [2] Disable DNS blocking"
    echo "  [b] Back"
    echo ""
    read -rp "  Choice: " c
    case "$c" in
        1) dns_block_enable ;;
        2) dns_block_disable ;;
        b|B) return ;;
    esac
    read -rp "  Press enter..." _
}


# --- Main ---
main() {
    if ! mkdir "$LOCK_DIR" 2>/dev/null; then
        print_error "Another instance is running"
        exit 1
    fi
    trap 'rmdir "$LOCK_DIR" 2>/dev/null; exit' EXIT

    if [[ $# -gt 0 ]]; then
        case "${1:-}" in
            --status)       run_status; exit 0 ;;
            --scan)         run_scan; exit 0 ;;
            --slay)         run_slay; exit 0 ;;
            --start-killer) start_killer; exit 0 ;;
            --stop-killer)  stop_killer; exit 0 ;;
            --help)         show_help; exit 0 ;;
            *)              print_error "Unknown option: $1"; exit 1 ;;
        esac
    fi

    while true; do
        show_menu
        read -r choice
        case $choice in
            1) run_status ;;
            2) run_scan ;;
            3) run_slay ;;
            4) start_killer; pause ;;
            5) stop_killer; pause ;;
            6) run_restore ;;
            7) show_help ;;
            8) show_mikey_findings; read -rp "Press enter..." _ ;;
            9) slay_mikey_findings; read -rp "Press enter..." _ ;;
           10) echo -e "\n${GREEN}Exiting.${RESET}"; exit 0 ;;
           11) run_dns_menu ;;
            q|Q) echo -e "\n${GREEN}Exiting.${RESET}"; exit 0 ;;
        *) echo -e "\n${RED}Invalid option${RESET}"; sleep 1 ;;
        esac
    done
}

main "$@"

# --- PowerKeeper soft-target recon ---
# No caps, not MIMD-protected — softest kill-decision target mapped this session
run_powerkeeper_recon() {
    clear
    print_header
    echo -e "${BOLD}${CYAN}=== PowerKeeper Recon ===${RESET}\n"

    local shell_type
    shell_type=$(check_shell)
    if [[ "$shell_type" == "none" ]]; then
        print_error "No rish or adb available"
        pause
        return
    fi

    local pid
    pid=$(exec_priv "pgrep -x powerkeeper" | tr -d '\n')
    if [[ -z "$pid" ]]; then
        print_warning "powerkeeper process not found"
        pause
        return
    fi

    print_success "powerkeeper PID: $pid"

    local threads
    threads=$(exec_priv "cat /proc/$pid/status" | grep Threads)
    local oom
    oom=$(exec_priv "cat /proc/$pid/oom_score_adj")
    local caps
    caps=$(exec_priv "cat /proc/$pid/status" | grep -E '^Cap')
    local cgroup
    cgroup=$(exec_priv "cat /proc/$pid/cgroup" | tr '\n' ' ')

    echo "  $threads"
    echo "  OOM score adj: $oom"
    echo "  Cgroup: $cgroup"
    echo "$caps" | while read -r line; do echo "  $line"; done

    echo ""
    print_info "IPowerMillet binder service (miui.powerkeeper.PowerMillet):"
    local dump
    dump=$(exec_priv "dumpsys activity service PowerKeeper" 2>/dev/null | head -20)
    if [[ -n "$dump" ]]; then
        echo "$dump" | sed 's/^/  /'
    else
        print_warning "No dumpsys output"
    fi

    echo ""
    print_warning "No caps, not MIMD-protected — fuzz/exhaustion still unverified"
    pause
}
