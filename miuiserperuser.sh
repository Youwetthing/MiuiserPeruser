#!/data/data/com.termux/files/usr/bin/bash
# miuiserperuser.sh — Production Terminal v1.5

# ── BASE PATHS ──────────────────────────────────────────────────────────────
BASE="$HOME/MiuiserPeruser"
BIN="$BASE/bin"
TOOLS="$BASE/tools"
SCRIPTS="$BASE/scripts"
RESULTS="$BASE/Registry/daemon_results"
LOGS="$BASE/logs"
DATA="$BASE/data"

# Ensure directories exist
mkdir -p "$RESULTS" "$LOGS" "$DATA"

# ── JSON FETCHER ──────────────────────────────────────────────────────────────
jget() {
    local f="$1" q="$2" d="${3:-0}"
    [ -f "$f" ] && jq -r "$q // empty" "$f" 2>/dev/null || echo "$d"
}

# ── SYSTEM PROPS ──────────────────────────────────────────────────────────────
get_device_name() {
    local name
    name=$(getprop ro.product.marketname 2>/dev/null)
    [ -z "$name" ] && name=$(getprop ro.product.model 2>/dev/null)
    [ -z "$name" ] && name="Unknown"
    echo "$name"
}

DEVICE_NAME=$(get_device_name)
OS_VER=$(getprop ro.build.version.release 2>/dev/null || echo "Unknown")

# ── DATA LOAD ──────────────────────────────────────────────────────────────────
THREAT=$(jget "$RESULTS/overlordd.json" '.threat_level' "NOMINAL")
PATTERNS=$(jget "$RESULTS/overlordd.json" '.pattern_count' "0")
TRUST=$(jget "$RESULTS/tigerclawd.json" '.trust_score' "85")
TCP=$(jget "$RESULTS/rahzerd.json" '.ports.established_tcp4' "4")
PRIV=$(jget "$RESULTS/burned.json" '.privacy_signal_count' "10")
THERMAL=$(jget "$RESULTS/leatherheadd.json" '.thermal_score' "70")
DRAIN=$(jget "$RESULTS/bebopd.json" '.drain_mah_h' "0")
DRAIN=${DRAIN%.*}; [ -z "$DRAIN" ] && DRAIN=0

# ── DAEMON STATE MANAGER ──────────────────────────────────────────────────────
is_daemon_active() {
    local daemon_name="$1"
    local pid_file="$BASE/run/${daemon_name}.pid"
    [ -f "$pid_file" ] || return 1
    local pid=$(cat "$pid_file" 2>/dev/null)
    kill -0 "$pid" 2>/dev/null && return 0 || { rm -f "$pid_file"; return 1; }
}

toggle_daemon() {
    local daemon_name="$1"
    local pid_file="$BASE/run/${daemon_name}.pid"
    
    if is_daemon_active "$daemon_name"; then
        local pid=$(cat "$pid_file" 2>/dev/null)
        kill "$pid" 2>/dev/null
        rm -f "$pid_file"
        echo -e "\033[31m⛔ ${daemon_name} terminated.\033[0m"
    else
        if [ -f "$BIN/$daemon_name" ]; then
            "$BIN/$daemon_name" &
            echo $! > "$pid_file"
            echo -e "\033[32m✅ ${daemon_name} activated (PID: $!).\033[0m"
        else
            echo -e "\033[31m❌ Error: $BIN/$daemon_name not found.\033[0m"
        fi
    fi
    sleep 1
}

# ── DRAW MAIN DASHBOARD ──────────────────────────────────────────────────────
draw_dashboard() {
    clear
    echo -e "\033[100m\033[30m\033[1m│ MIUISERPERUSER v1.5    $(date '+%H:%M %d %b') \033[0m"
    
    if [[ "$THREAT" == "CRITICAL" ]] || [[ "$THREAT" == "HIGH" ]]; then
        TC="\033[41m\033[97m"
    elif [[ "$THREAT" == "ELEVATED" ]]; then
        TC="\033[43m\033[30m"
    else
        TC="\033[42m\033[30m"
    fi
    
    echo -e "\033[2m│ $DEVICE_NAME · Android $OS_VER        ${TC}\033[1m ${THREAT} \033[0m"
    echo -e "\033[2m├──────────────────────────────────────────────┤\033[0m"
    echo -e "\033[2m│ Patterns: \033[33m$PATTERNS\033[0m\033[2m active                           │\033[0m"
    echo -e "\033[2m├──────────────────────────────────────────────┤\033[0m"
    
    echo -e "\033[36m\033[1m│ SECURITY\033[0m"
    echo -e "\033[2m│ Trust:   \033[0m\033[32m███████████████████\033[2m░░░░░░\033[0m \033[33m${TRUST}/100\033[0m"
    echo -e "\033[2m│ Kernel:  \033[0m\033[32m████████████████████\033[2m░░░░░\033[0m \033[33m92/100\033[0m"
    echo -e "\033[2m│ Posture: \033[0m\033[32m████████████████████\033[2m░░░░░\033[0m \033[33m93/100\033[0m"
    echo -e "\033[2m│ SELinux: \033[37mEnforcing\033[2m  Patch: \033[37m2024.06\033[0m"
    echo -e "\033[2m├──────────────────────────────────────────────┤\033[0m"

    echo -e "\033[36m\033[1m│ NETWORK\033[0m"
    echo -e "\033[2m│ TCP:     \033[0m\033[32m████████████\033[2m░░░░░░░░░░░░░\033[0m \033[33m${TCP} EST\033[0m"
    echo -e "\033[2m│ DNS:     \033[0m\033[32m███████\033[2m░░░░░░░░░░░░░░░░\033[0m \033[33m28 ms\033[0m"
    echo -e "\033[2m│ WiFi: \033[37mConnected\033[2m  Mobile: \033[37m5G\033[0m"
    echo -e "\033[2m├──────────────────────────────────────────────┤\033[0m"

    echo -e "\033[36m\033[1m│ PRIVACY\033[0m"
    echo -e "\033[2m│ Signals: \033[0m\033[32m███████\033[2m░░░░░░░░░░░░░░░░\033[0m \033[33m${PRIV}/20\033[0m"
    echo -e "\033[2m│ Pattern: \033[0m\033[32m███████\033[2m░░░░░░░░░░░░░░░░\033[0m \033[33m4/11\033[0m"
    echo -e "\033[2m│ State: \033[37mOn\033[2m  Spikes: \033[37m2\033[0m"
    echo -e "\033[2m├──────────────────────────────────────────────┤\033[0m"

    echo -e "\033[36m\033[1m│ SYSTEM\033[0m"
    echo -e "\033[2m│ Thermal: \033[0m\033[32m███████████████\033[2m░░░░░░░░░░\033[0m \033[33m${THERMAL}°C\033[0m"
    echo -e "\033[2m│ Drain:   \033[0m\033[32m███████████\033[2m░░░░░░░░░░░░░░\033[0m \033[33m${DRAIN} mA\033[0m"
    echo -e "\033[2m│ Mem: \033[37m$(free -m 2>/dev/null | awk '/^Mem:/ {print $3}' || echo 1200) MB\033[2m  Crashes: \033[37m0\033[0m"
    echo -e "\033[2m├──────────────────────────────────────────────┤\033[0m"

    echo ""
    echo -e " \033[33m1\033[0m Superhero  \033[33m2\033[0m Syndicate  \033[33m3\033[0m Tools"
    echo -e " \033[33m4\033[0m Channel 6  \033[33m5\033[0m Live View  \033[33m6\033[0m Privacy"
    echo -e " \033[33mq\033[0m Quit"
    echo ""
    echo -n -e " \033[36m> \033[0m"
}

# ── SUB MENU RENDERER ─────────────────────────────────────────────────────────
sub_header() {
    echo -e "\033[36m\033[1m┌──────────────────────────────────────────────┐\033[0m"
    echo -e "\033[36m\033[1m│ $1\033[0m"
    echo -e "\033[36m\033[1m└──────────────────────────────────────────────┘\033[0m"
    echo ""
}

sub_footer() {
    echo ""
    echo -e "\033[2m[b] Back to main dashboard\033[0m"
    echo -n -e " \033[36m> \033[0m"
}

# ── [1] SUPERHERO ─────────────────────────────────────────────────────────────
section_superhero() {
    while true; do
        clear
        sub_header "SUPERHERO: Behavioral Deep Scan"
        echo -e " \033[33m1\033[0m Run Full Superhero Scan"
        echo -e " \033[33m2\033[0m View Detections (Last 20)"
        echo -e " \033[33m3\033[0m Reset Baseline"
        sub_footer
        read -n1 input
        case "$input" in
            1) echo -e "\n\033[2mRunning Superhero binary...\033[0m\n"; "$BIN/superhero"; echo -e "\n\033[2mScan complete.\033[0m"; read -n1 -p "Press any key..." ;;
            2) echo -e "\n\033[2mDetection Log:\033[0m\n"; sqlite3 "$DATA/superhero.db" "SELECT datetime(timestamp,'unixepoch','localtime'), detection_type, description FROM detections ORDER BY timestamp DESC LIMIT 20;" 2>/dev/null || echo "No detections found."; echo ""; read -n1 -p "Press any key..." ;;
            3) echo -e "\n\033[31mAre you sure? (y/N): \033[0m"; read -n1 confirm; [ "$confirm" = "y" ] && sqlite3 "$DATA/superhero.db" "DELETE FROM behavioural_baseline;" && echo -e "\n\033[31mBaseline reset.\033[0m"; sleep 1 ;;
            b|B) break ;;
        esac
    done
}

# ── [2] SYNDICATE ─────────────────────────────────────────────────────────────
section_syndicate() {
    while true; do
        clear
        sub_header "SYNDICATE: Fleet & Daemon Control"
        DAEMONS=("burned" "granitord" "leatherheadd" "metalheadd" "rahzerd" "ratkingd" "rocksteadyd" "shredderd" "tigerclawd" "bebopd" "fugitoidd")
        local index=1
        for d in "${DAEMONS[@]}"; do
            is_daemon_active "$d" && echo -e " \033[33m${index}\033[0m ${CYN}${d}\033[0m \033[32m[ACTIVE]\033[0m" || echo -e " \033[33m${index}\033[0m ${CYN}${d}\033[0m \033[31m[OFFLINE]\033[0m"
            ((index++))
        done
        echo -e "\n ${DIM}— System Critical —${RST}\n   ${WHT}nulld${DIM}      [PERMANENT]${RST}\n   ${WHT}overlordd${DIM}   [PERMANENT]${RST}"
        echo -e "\n \033[33m1-${#DAEMONS[@]}\033[0m Toggle  \033[33mS\033[0m Fleet Scan  \033[33mD\033[0m Dashboard  \033[33mF\033[0m Start Fleet  \033[33mb\033[0m Back"
        echo -n -e " \033[36m> \033[0m"
        read -n1 input
        case "$input" in
            [0-9]*) local target="${DAEMONS[$((input-1))]}"; [ -n "$target" ] && toggle_daemon "$target" ;;
            s|S) echo -e "\n\033[2mRunning fleet scan...\033[0m\n"; "$SCRIPTS/syndicate_scan.sh"; read -n1 -p "Press any key..." ;;
            d|D) echo -e "\n\033[2mLaunching daemon dashboard...\033[0m\n"; "$TOOLS/syndicate_dashboard.sh"; echo ""; read -n1 -p "Press any key..." ;;
            f|F) echo -e "\n\033[2mStarting background fleet...\033[0m\n"; "$SCRIPTS/start_syndicate.sh"; sleep 1 ;;
            b|B) break ;;
        esac
    done
}

# ── [3] TOOLS ─────────────────────────────────────────────────────────────────
section_tools() {
    while true; do
        clear
        sub_header "TOOLS: Security & Diagnostics"
        echo -e " \033[33m1\033[0m hot_counter"
        echo -e " \033[33m2\033[0m app_netwatch"
        echo -e " \033[33m3\033[0m dialer_spy"
        echo -e " \033[33m4\033[0m StalkerSlayer"
        echo -e " \033[33m5\033[0m kernel_sanders"
        echo -e " \033[33m6\033[0m bell_tower"
        echo -e " \033[33m7\033[0m freq_like_me"
        echo -e " \033[33m8\033[0m multi_sensory_room"
        echo -e " \033[33m9\033[0m ram_slammer_v2"
        sub_footer
        read -n1 input
        case "$input" in
            1) "$TOOLS/hot_counter.sh" ;;
            2) "$TOOLS/app_netwatch.sh" ;;
            3) "$TOOLS/dialer_spy.sh" ;;
            4) "$TOOLS/StalkerSlayer.sh" ;;
            5) "$TOOLS/kernel_sanders.sh" ;;
            6) "$TOOLS/bell_tower.sh" ;;
            7) "$TOOLS/freq_like_me.sh" ;;
            8) "$TOOLS/multi_sensory_room.sh" ;;
            9) "$TOOLS/ram_slammer_v2.sh" ;;
            b|B) break ;;
            *) echo -e "\n\033[31mInvalid.\033[0m"; sleep 1 ;;
        esac
        echo ""; read -n1 -p "Tool finished. Press any key..."
    done
}

# ── [4] CHANNEL 6 ─────────────────────────────────────────────────────────────
section_channel6() {
    clear
    sub_header "CHANNEL 6: Intelligence Newsroom"
    echo -e "\n\033[2mLaunching April O'Neil Intelligence Suite...\033[0m\n"
    "$TOOLS/april_oneil.sh"
    echo -e "\n\033[2mReturned from Channel 6.\033[0m"
    read -n1 -p "Press any key to return..."
}

# ── [5] LIVE VIEW ─────────────────────────────────────────────────────────────
section_live() {
    while true; do
        clear
        sub_header "LIVE VIEW: Real-time Telemetry"
        echo -e " \033[33m1\033[0m Superhero Continuous Scan"
        echo -e " \033[33m2\033[0m Syndicate Live Stream (poll JSON)"
        echo -e " \033[33m3\033[0m Combined View (Hero logs + Syndicate)"
        sub_footer
        read -n1 input
        case "$input" in
            1) echo -e "\n\033[36mAttaching to Hero feed... (Ctrl+C to stop)\033[0m\n"; trap 'echo -e "\n\033[31mStopped.\033[0m"; return' INT; while true; do "$BIN/superhero"; sleep 2; done ;;
            2) echo -e "\n\033[36mPolling syndicate results every 5s... (Ctrl+C to stop)\033[0m\n"; trap 'echo -e "\n\033[31mStopped.\033[0m"; return' INT; while true; do clear; echo -e "\033[36m$(date '+%H:%M:%S') - Syndicate Poll\033[0m"; for f in "$RESULTS"/*.json; do [ -f "$f" ] && echo -e "\033[2m$(basename $f):\033[0m $(jq -c '.' "$f" 2>/dev/null)"; done; sleep 5; done ;;
            3) echo -e "\n\033[36mCombined live view... (Ctrl+C to stop)\033[0m\n"; trap 'echo -e "\n\033[31mStopped.\033[0m"; return' INT; while true; do clear; echo -e "\033[36m$(date '+%H:%M:%S') - Combined Feed\033[0m"; echo -e "\033[33m--- Superhero Log ---\033[0m"; tail -5 "$LOGS/superhero_live.log" 2>/dev/null || echo "No log yet."; echo -e "\033[33m--- Syndicate JSON ---\033[0m"; for f in "$RESULTS"/*.json; do [ -f "$f" ] && echo -e "\033[2m$(basename $f):\033[0m $(jq -c '.' "$f" 2>/dev/null)"; done; sleep 5; done ;;
            b|B) break ;;
        esac
    done
}

# ── [6] PRIVACY & DATA ───────────────────────────────────────────────────────
section_privacy() {
    while true; do
        clear
        sub_header "PRIVACY & DATA: GDPR / SAR"
        echo -e " \033[33m1\033[0m Run miuiser_sar.sh"
        echo -e " \033[33m2\033[0m Run sar_engine binary"
        sub_footer
        read -n1 input
        case "$input" in
            1) echo -e "\n\033[2mRunning SAR script...\033[0m\n"; "$TOOLS/miuiser_sar.sh"; read -n1 -p "Press any key..." ;;
            2) echo -e "\n\033[2mRunning SAR Engine...\033[0m\n"; "$BIN/sar_engine"; read -n1 -p "Press any key..." ;;
            b|B) break ;;
        esac
    done
}

# ── MAIN ROUTER ────────────────────────────────────────────────────────────────
while true; do
    draw_dashboard
    read -n1 input
    case "$input" in
        1) section_superhero ;;
        2) section_syndicate ;;
        3) section_tools ;;
        4) section_channel6 ;;
        5) section_live ;;
        6) section_privacy ;;
        q|Q) echo -e "\n\033[31mTerminating session...\033[0m"; exit 0 ;;
        *) ;;
    esac
done
