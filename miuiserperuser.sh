#!/data/data/com.termux/files/usr/bin/bash
# miuiserperuser.sh — Production Terminal v1.5
# Complete integrated dashboard.

# ── BASE PATHS ──────────────────────────────────────────────────────────────
BASE="$HOME/MiuiserPeruser"
BIN="$BASE/bin"
TOOLS="$BASE/tools"
SCRIPTS="$BASE/scripts"
RESULTS="$BASE/Registry/daemon_results"
LOGS="$BASE/logs"
DATA="$BASE/data"

mkdir -p "$RESULTS" "$LOGS" "$DATA"

jget() {
    local f="$1" q="$2" d="${3:-0}"
    [ -f "$f" ] && jq -r "$q // empty" "$f" 2>/dev/null || echo "$d"
}

get_device_name() {
    local name
    name=$(getprop ro.product.marketname 2>/dev/null)
    [ -z "$name" ] && name=$(getprop ro.product.model 2>/dev/null)
    [ -z "$name" ] && name="Unknown"
    echo "$name"
}

DEVICE_NAME=$(get_device_name)
OS_VER=$(getprop ro.build.version.release 2>/dev/null || echo "Unknown")

load_data() {
    THREAT=$(jget "$RESULTS/overlordd.json" '.threat_level' "NOMINAL")
    PATTERNS=$(jget "$RESULTS/overlordd.json" '.pattern_count' "0")
    TRUST=$(jget "$RESULTS/tigerclawd.json" '.trust_score' "85")
    TCP=$(jget "$RESULTS/rahzerd.json" '.ports.established_tcp4' "4")
    PRIV=$(jget "$RESULTS/burned.json" '.privacy_signal_count' "10")
    THERMAL=$(jget "$RESULTS/leatherheadd.json" '.thermal_score' "70")
    DRAIN=$(jget "$RESULTS/bebopd.json" '.drain_mah_h' "0")
    DRAIN=${DRAIN%.*}; [ -z "$DRAIN" ] && DRAIN=0
    K_SCORE=$(jget "$RESULTS/shredderd.json" '.integrity.score' "92")
    G_SCORE=$(jget "$RESULTS/granitord.json" '.posture.score' "93")
    SELINUX=$(jget "$RESULTS/tigerclawd.json" '.device.selinux_enforcing' "Enforcing")
    PATCH=$(jget "$RESULTS/tigerclawd.json" '.device.security_patch' "?")
    DNS_MS=$(jget "$RESULTS/rahzerd.json" '.dns.latency_ms' "?")
    WIFI=$(jget "$RESULTS/rahzerd.json" '.wifi.connected' "0")
    SPIKES=$(jget "$RESULTS/nulld.json" '.total_spike_events' "0")
    MEM=$(jget "$RESULTS/ratkingd.json" '.memory.available_mb' "?")
    CRASHES=$(jget "$RESULTS/fugitoidd.json" '.crashes' "0")
    CYCLES=$(sqlite3 "$DATA/superhero.db" "SELECT COUNT(*) FROM scan_history;" 2>/dev/null || echo "0")
    BASELINE=$(sqlite3 "$DATA/superhero.db" "SELECT COUNT(*) FROM behavioural_baseline;" 2>/dev/null || echo "0")
}

bar() {
    local val=$1 max=${2:-100} width=${3:-20}
    local filled=$(( val * width / max ))
    [ $filled -gt $width ] && filled=$width
    [ $filled -lt 0 ] && filled=0
    local empty=$(( width - filled ))
    local i=0
    printf "\033[32m"
    while [ $i -lt $filled ]; do printf "█"; i=$((i+1)); done
    printf "\033[2m"
    i=0
    while [ $i -lt $empty ]; do printf "░"; i=$((i+1)); done
    printf "\033[0m"
}

draw_dashboard() {
    load_data
    clear

    if [[ "$THREAT" == "CRITICAL" ]] || [[ "$THREAT" == "HIGH" ]]; then
        TC="\033[41m\033[97m"
    elif [[ "$THREAT" == "ELEVATED" ]]; then
        TC="\033[43m\033[30m"
    else
        TC="\033[42m\033[30m"
    fi

    local wifi_str; [ "$WIFI" = "1" ] && wifi_str="Connected" || wifi_str="Disconnected"
    local baseline_str; [ "$BASELINE" -gt 0 ] && baseline_str="ACTIVE" || baseline_str="LEARNING ${CYCLES}/12"

    echo -e "\033[100m\033[30m\033[1m MIUISERPERUSER v1.5          $(date '+%H:%M  %d %b %Y') \033[0m"
    echo -e "\033[2m $DEVICE_NAME · Android $OS_VER · Superhero: $baseline_str    ${TC}\033[1m ${THREAT} \033[0m"
    echo -e "\033[2m Overlordd: \033[33m${PATTERNS}\033[2m active patterns\033[0m"
    echo -e "\033[2m────────────────────────────────────────────────\033[0m"

    echo -e "\033[96m\033[1m SECURITY\033[0m"
    echo -e " Trust   $(bar $TRUST)  \033[33m${TRUST}/100\033[0m"
    echo -e " Kernel  $(bar $K_SCORE)  \033[33m${K_SCORE}/100\033[0m"
    echo -e " Posture $(bar $G_SCORE)  \033[33m${G_SCORE}/100\033[0m"
    echo -e " \033[2mSELinux: \033[37m${SELINUX}\033[2m  Patch: \033[37m${PATCH}\033[0m"
    echo -e "\033[2m────────────────────────────────────────────────\033[0m"

    echo -e "\033[96m\033[1m NETWORK\033[0m"
    echo -e " TCP     $(bar $TCP 100)  \033[33m${TCP} conn\033[0m"
    echo -e " DNS     $(bar ${DNS_MS:-0} 200)  \033[33m${DNS_MS}ms\033[0m"
    echo -e " \033[2mWiFi: \033[37m${wifi_str}\033[2m  Idle spikes: \033[37m${SPIKES}\033[0m"
    echo -e "\033[2m────────────────────────────────────────────────\033[0m"

    echo -e "\033[96m\033[1m PRIVACY\033[0m"
    echo -e " Signals $(bar $PRIV 20)  \033[33m${PRIV}/20\033[0m"
    echo -e " Pattern $(bar $PATTERNS 11)  \033[33m${PATTERNS}/11\033[0m"
    echo -e "\033[2m────────────────────────────────────────────────\033[0m"

    echo -e "\033[96m\033[1m SYSTEM\033[0m"
    echo -e " Thermal $(bar $THERMAL)  \033[33m${THERMAL}/100\033[0m"
    echo -e " Drain   $(bar $DRAIN 500)  \033[33m${DRAIN}mAh/hr\033[0m"
    echo -e " \033[2mMemory: \033[37m${MEM}MB\033[2m  Crashes: \033[37m${CRASHES}\033[0m"
    echo -e "\033[2m────────────────────────────────────────────────\033[0m"

    echo ""
    echo -e " \033[33m1\033[0m Superhero  \033[33m2\033[0m Syndicate  \033[33m3\033[0m Tools"
    echo -e " \033[33m4\033[0m Channel 6  \033[33m5\033[0m Live View  \033[33m6\033[0m Privacy"
    echo -e " \033[33mr\033[0m Refresh    \033[33mq\033[0m Quit"
    echo ""
    echo -n -e " \033[96m❯ \033[0m"
}

sub_header() {
    clear
    echo -e "\033[96m\033[1m────────────────────────────────────────────────\033[0m"
    echo -e "\033[96m\033[1m $1\033[0m"
    echo -e "\033[96m\033[1m────────────────────────────────────────────────\033[0m"
    echo ""
}

section_superhero() {
    while true; do
        sub_header "SUPERHERO — Behavioural Deep Scan"
        local cycles=$(sqlite3 "$DATA/superhero.db" "SELECT COUNT(*) FROM scan_history;" 2>/dev/null || echo "0")
        local baseline=$(sqlite3 "$DATA/superhero.db" "SELECT COUNT(*) FROM behavioural_baseline;" 2>/dev/null || echo "0")
        if [ "$baseline" -gt 0 ]; then
            echo -e " \033[32m✔ Baseline active — anomaly detection ON\033[0m"
        else
            echo -e " \033[33m⚑ Learning mode — $((12 - cycles)) more cycles needed\033[0m"
        fi
        echo ""
        echo -e " \033[33m1\033[0m Run scan"
        echo -e " \033[33m2\033[0m View last 20 detections"
        echo -e " \033[33m3\033[0m Reset baseline"
        echo -e " \033[33mb\033[0m Back"
        echo ""
        read -n1 -p $' \033[96m❯ \033[0m' input
        case "$input" in
            1) echo -e "\n\n\033[2mRunning superhero scan...\033[0m\n"
               RISH_APPLICATION_ID=com.termux "$BIN/superhero" 2>&1
               echo ""; read -n1 -p "Press any key..." ;;
            2) echo -e "\n"
               sqlite3 "$DATA/superhero.db" \
                   "SELECT datetime(timestamp,'unixepoch','localtime'), detection_type, description FROM detections ORDER BY timestamp DESC LIMIT 20;" \
                   2>/dev/null || echo "No detections found."
               echo ""; read -n1 -p "Press any key..." ;;
            3) echo -e "\n\033[31mReset baseline? (y/N): \033[0m"
               read -n1 confirm
               [ "$confirm" = "y" ] && \
                   sqlite3 "$DATA/superhero.db" "DELETE FROM behavioural_baseline;" && \
                   echo -e "\n\033[31mBaseline reset.\033[0m"
               sleep 1 ;;
            b|B) break ;;
        esac
    done
}

section_syndicate() {
    while true; do
        sub_header "SYNDICATE — Fleet Control"
        echo -e " \033[33m1\033[0m Run syndicate scan"
        echo -e " \033[33m2\033[0m Daemon toggle dashboard"
        echo -e " \033[33m3\033[0m Start background fleet"
        echo -e " \033[33mb\033[0m Back"
        echo ""
        read -n1 -p $' \033[96m❯ \033[0m' input
        case "$input" in
            1) echo -e "\n"; "$SCRIPTS/syndicate_scan.sh"; read -n1 -p "Press any key..." ;;
            2) "$TOOLS/syndicate_dashboard.sh" ;;
            3) echo -e "\n\033[2mStarting background fleet...\033[0m\n"
               "$SCRIPTS/start_syndicate.sh"; sleep 1 ;;
            b|B) break ;;
        esac
    done
}

section_tools() {
    while true; do
        sub_header "TOOLS — Security & Diagnostics"
        echo -e " \033[33m1\033[0m hot_counter        \033[2mThermal truth\033[0m"
        echo -e " \033[33m2\033[0m app_netwatch       \033[2mPer-app network\033[0m"
        echo -e " \033[33m3\033[0m dialer_spy         \033[2mCall monitor\033[0m"
        echo -e " \033[33m4\033[0m StalkerSlayer      \033[2mTelemetry blocker\033[0m"
        echo -e " \033[33m5\033[0m kernel_sanders     \033[2mKernel audit\033[0m"
        echo -e " \033[33m6\033[0m bell_tower         \033[2mAlert monitor\033[0m"
        echo -e " \033[33m7\033[0m freq_like_me       \033[2mCPU frequencies\033[0m"
        echo -e " \033[33m8\033[0m multi_sensory_room \033[2mSensor registry\033[0m"
        echo -e " \033[33m9\033[0m ram_slammer_v2     \033[2mMemory & process\033[0m"
        echo -e " \033[33mb\033[0m Back"
        echo ""
        read -n1 -p $' \033[96m❯ \033[0m' input
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
        esac
    done
}

section_channel6() {
    "$TOOLS/april_oneil.sh"
}

section_live() {
    while true; do
        sub_header "LIVE VIEW — Real-time Feed"
        echo -e " \033[33m1\033[0m Superhero continuous scan"
        echo -e " \033[33m2\033[0m Syndicate live stream"
        echo -e " \033[33m3\033[0m Combined view"
        echo -e " \033[33mb\033[0m Back"
        echo ""
        read -n1 -p $' \033[96m❯ \033[0m' input
        case "$input" in
            1)
                echo -e "\n\033[36mSuperhero live — Ctrl+C to stop\033[0m\n"
                trap 'trap - INT; echo -e "\n\033[31mStopped.\033[0m"; return' INT
                while true; do
                    RISH_APPLICATION_ID=com.termux "$BIN/superhero" 2>&1 | \
                        grep --line-buffered -E "LEO|DON|RAPH|MIKEY|CASEY|finding|WARN"
                    echo -e "\033[2m── cycle complete · restarting ──\033[0m"
                    sleep 3
                done ;;
            2)
                echo -e "\n\033[36mSyndicate live — Ctrl+C to stop\033[0m\n"
                trap 'trap - INT; echo -e "\n\033[31mStopped.\033[0m"; return' INT
                while true; do
                    clear
                    echo -e "\033[96m$(date '+%H:%M:%S') — Syndicate Poll\033[0m"
                    echo -e "\033[2m────────────────────────────────────────────────\033[0m"
                    for d in burned granitord leatherheadd metalheadd rahzerd \
                              ratkingd rocksteadyd shredderd tigerclawd bebopd \
                              fugitoidd overlordd; do
                        local f="$RESULTS/${d}.json"
                        if [ -f "$f" ]; then
                            local ts
                            ts=$(jget "$f" '.timestamp' '?' | grep -oE '[0-9]{2}:[0-9]{2}' | head -1)
                            echo -e " \033[32m●\033[0m \033[96m${d}\033[2m  ${ts}\033[0m"
                        else
                            echo -e " \033[2m○ ${d}  no data\033[0m"
                        fi
                    done
                    sleep 5
                done ;;
            3)
                echo -e "\n\033[36mCombined live — Ctrl+C to stop\033[0m\n"
                trap 'trap - INT; kill $hero_pid 2>/dev/null; echo -e "\n\033[31mStopped.\033[0m"; return' INT
                RISH_APPLICATION_ID=com.termux "$BIN/superhero" \
                    >> "$LOGS/superhero_live.log" 2>&1 &
                local hero_pid=$!
                while true; do
                    clear
                    echo -e "\033[96m$(date '+%H:%M:%S') — Combined Feed\033[0m"
                    echo -e "\033[33m── Superhero ──\033[0m"
                    tail -4 "$LOGS/superhero_live.log" 2>/dev/null | \
                        grep -E "LEO|DON|RAPH|MIKEY|finding" || echo -e "\033[2mno output yet\033[0m"
                    echo -e "\033[33m── Syndicate ──\033[0m"
                    for d in rahzerd tigerclawd nulld overlordd; do
                        local f="$RESULTS/${d}.json"
                        [ -f "$f" ] && echo -e " \033[32m●\033[0m \033[96m${d}\033[0m" || \
                            echo -e " \033[2m○ ${d}\033[0m"
                    done
                    sleep 5
                done ;;
            b|B) break ;;
        esac
    done
}

section_privacy() {
    while true; do
        sub_header "PRIVACY & DATA — GDPR / SAR"
        echo -e " \033[33m1\033[0m Data inventory"
        echo -e " \033[33m2\033[0m Export my data (SAR)"
        echo -e " \033[33m3\033[0m Purge data"
        echo -e " \033[33m4\033[0m Consent records"
        echo -e " \033[33m5\033[0m Revoke consent"
        echo -e " \033[33mb\033[0m Back"
        echo ""
        read -n1 -p $' \033[96m❯ \033[0m' input
        case "$input" in
            1) echo ""; "$BIN/sar_engine" --inventory; echo ""; read -n1 -p "Press any key..." ;;
            2) echo ""; "$BIN/sar_engine" --export; echo ""; read -n1 -p "Press any key..." ;;
            3) echo ""; "$BIN/sar_engine" --purge; echo ""; read -n1 -p "Press any key..." ;;
            4) echo ""; "$BIN/sar_engine" --consent --list; echo ""; read -n1 -p "Press any key..." ;;
            5) echo ""; "$BIN/sar_engine" --consent --revoke all; echo ""; read -n1 -p "Press any key..." ;;
            b|B) break ;;
        esac
    done
}

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
        r|R) continue ;;
        q|Q) echo -e "\n\033[31mTerminating session...\033[0m"; exit 0 ;;
    esac
done
