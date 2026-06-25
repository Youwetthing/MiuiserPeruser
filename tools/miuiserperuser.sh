#!/data/data/com.termux/files/usr/bin/bash
# miuiserperuser.sh — Main Hub
# Entry point for all MiuiserPeruser functionality

BASE="$HOME/MiuiserPeruser"
BIN="$BASE/bin"
TOOLS="$BASE/tools"
RESULTS="$BASE/Registry/daemon_results"

# ── Colours ───────────────────────────────────────────────────────────────────
BGBLUE='\033[44m'; BGRED='\033[41m'; BGYEL='\033[43m'
CYN='\033[96m'; YEL='\033[93m'; RED='\033[91m'; GRN='\033[32m'
WHT='\033[97m'; DIM='\033[2m'; BOLD='\033[1m'; RST='\033[0m'

# ── Device fingerprint ────────────────────────────────────────────────────────
get_device() {
    local f="$RESULTS/tigerclawd.json"
    [ -f "$f" ] && jq -r '.device.codename // "unknown"' "$f" 2>/dev/null || echo "unknown"
}

get_hyperos() {
    local f="$RESULTS/tigerclawd.json"
    [ -f "$f" ] && jq -r '.device.hyperos_version // "?"' "$f" 2>/dev/null || echo "?"
}

get_threat() {
    local f="$RESULTS/overlordd.json"
    [ -f "$f" ] && jq -r '.threat_level // "UNKNOWN"' "$f" 2>/dev/null || echo "UNKNOWN"
}

get_threat_color() {
    case "$1" in
        CRITICAL) echo "$BGRED$WHT" ;;
        HIGH)     echo "$BGRED$WHT" ;;
        ELEVATED) echo "$BGYEL\033[30m" ;;
        GUARDED)  echo "$BGYEL\033[30m" ;;
        NOMINAL)  echo "\033[42m\033[30m" ;;
        *)        echo "$DIM" ;;
    esac
}

get_superhero_cycles() {
    sqlite3 "$BASE/data/superhero.db" \
        "SELECT COUNT(*) FROM scan_history;" 2>/dev/null || echo "0"
}

get_baseline_status() {
    local count=$(sqlite3 "$BASE/data/superhero.db" \
        "SELECT COUNT(*) FROM behavioural_baseline;" 2>/dev/null || echo "0")
    [ "$count" -gt 0 ] && echo "ACTIVE" || echo "LEARNING"
}

# ── Masthead ──────────────────────────────────────────────────────────────────
masthead() {
    clear
    local device=$(get_device)
    local hyperos=$(get_hyperos)
    local threat=$(get_threat)
    local threat_col=$(get_threat_color "$threat")
    local cycles=$(get_superhero_cycles)
    local baseline=$(get_baseline_status)
    local now=$(date '+%H:%M  %d %b %Y')

    printf "${BGBLUE}${WHT}${BOLD}"
    printf " %-38s %s${RST}\n" "MIUISERPERUSER  v1.2" "$now"
    printf "${BGBLUE}${CYN}"
    printf " %-38s %s${RST}\n" "Device: ${device} | ${hyperos}" "Superhero: ${cycles}/12 [${baseline}]"
    printf "${BGBLUE}${YEL}"
    printf " %-30s${RST} ${threat_col}${BOLD} THREAT: %-10s ${RST}\n" \
        "MiuiserPeruser Intelligence Bureau" "$threat"
    echo ""
}

# ── Main menu ─────────────────────────────────────────────────────────────────
main_menu() {
    masthead

    # Quick status bar
    local rahzerd_f="$RESULTS/rahzerd.json"
    local nulld_f="$RESULTS/nulld.json"
    if [ -f "$rahzerd_f" ]; then
        local wifi=$(jq -r '.wifi.connected // -1' "$rahzerd_f" 2>/dev/null)
        local rat=$(jq -r '.mobile.rat // "?"' "$rahzerd_f" 2>/dev/null)
        local dns=$(jq -r '.dns.latency_ms // "?"' "$rahzerd_f" 2>/dev/null)
        local tcp=$(( $(jq -r '.ports.established_tcp4 // 0' "$rahzerd_f" 2>/dev/null) + $(jq -r '.ports.established_tcp6 // 0' "$rahzerd_f" 2>/dev/null) ))
        printf "  ${DIM}Network: ${WHT}%s${DIM}  Mobile: ${WHT}%s${DIM}  DNS: ${WHT}%sms${DIM}  Connections: ${WHT}%s${RST}\n" \
            "$([ "$wifi" = "1" ] && echo "WiFi" || echo "No WiFi")" "$rat" "$dns" "$tcp"
    fi
    if [ -f "$nulld_f" ]; then
        local spikes=$(jq -r '.total_spike_events // 0' "$nulld_f" 2>/dev/null)
        local screen=$(jq -r '.screen // "?"' "$nulld_f" 2>/dev/null)
        printf "  ${DIM}Screen: ${WHT}%s${DIM}  Idle spikes: ${WHT}%s${RST}\n\n" "$screen" "$spikes"
    else
        echo ""
    fi

    printf "  ${YEL}[1]${RST} ${BOLD}${WHT}Superhero${RST}        ${DIM}Behavioural deep scan${RST}\n"
    printf "  ${YEL}[2]${RST} ${BOLD}${WHT}Syndicate${RST}        ${DIM}Fleet scan + daemon control${RST}\n"
    printf "  ${YEL}[3]${RST} ${BOLD}${WHT}Tools${RST}            ${DIM}Security & privacy tools${RST}\n"
    printf "  ${YEL}[4]${RST} ${BOLD}${WHT}Channel 6${RST}        ${DIM}Intelligence newsroom${RST}\n"
    printf "  ${YEL}[5]${RST} ${BOLD}${WHT}Live View${RST}        ${DIM}Real-time scan feed${RST}\n"
    printf "  ${YEL}[6]${RST} ${BOLD}${WHT}Privacy & Data${RST}   ${DIM}GDPR / Subject Access Request${RST}\n"
    printf "  ${CYN}[q]${RST} ${DIM}Quit${RST}\n\n"

    read -r -p "$(printf "  ${CYN}Choice: ${RST}")" choice
    handle_choice "$choice"
}

# ── Superhero ─────────────────────────────────────────────────────────────────
section_superhero() {
    clear
    masthead
    printf "  ${BOLD}${WHT}SUPERHERO — BEHAVIOURAL SCAN${RST}\n\n"
    local cycles=$(get_superhero_cycles)
    local baseline=$(get_baseline_status)
    printf "  ${DIM}Cycles completed: ${WHT}%s/12${DIM}  Status: ${WHT}%s${RST}\n\n" \
        "$cycles" "$baseline"

    if [ "$baseline" = "LEARNING" ]; then
        local remaining=$((12 - cycles))
        printf "  ${YEL}⚑ Learning mode — %d more cycles needed${RST}\n" "$remaining"
        printf "  ${DIM}Superhero learns normal behaviour before flagging anomalies.${RST}\n\n"
    fi

    printf "  ${YEL}[1]${RST} Run scan now\n"
    printf "  ${YEL}[2]${RST} View last results\n"
    printf "  ${YEL}[b]${RST} Back\n\n"
    read -r -p "$(printf "  ${CYN}Choice: ${RST}")" c
    case "$c" in
        1) RISH_APPLICATION_ID=com.termux "$BIN/superhero" 2>&1
           printf "\n  ${DIM}Press any key...${RST}"; read -n1
           section_superhero ;;
        2) sqlite3 "$BASE/data/superhero.db" \
               "SELECT timestamp, detection_type, description FROM detections ORDER BY timestamp DESC LIMIT 20;" \
               2>/dev/null | head -40
           printf "\n  ${DIM}Press any key...${RST}"; read -n1
           section_superhero ;;
        b|B) main_menu ;;
        *) main_menu ;;
    esac
}

# ── Syndicate ─────────────────────────────────────────────────────────────────
section_syndicate() {
    clear
    masthead
    printf "  ${BOLD}${WHT}SYNDICATE — FLEET CONTROL${RST}\n\n"
    printf "  ${YEL}[1]${RST} Run syndicate scan\n"
    printf "  ${YEL}[2]${RST} Daemon dashboard (toggle on/off)\n"
    printf "  ${YEL}[3]${RST} Start background fleet\n"
    printf "  ${YEL}[b]${RST} Back\n\n"
    read -r -p "$(printf "  ${CYN}Choice: ${RST}")" c
    case "$c" in
        1) "$BASE/scripts/syndicate_scan.sh" ; section_syndicate ;;
        2) "$TOOLS/syndicate_dashboard.sh"   ; section_syndicate ;;
        3) "$BASE/scripts/start_syndicate.sh"; section_syndicate ;;
        b|B) main_menu ;;
        *) main_menu ;;
    esac
}

# ── Tools ─────────────────────────────────────────────────────────────────────
section_tools() {
    clear
    masthead
    printf "  ${BOLD}${WHT}TOOLS${RST}\n\n"
    printf "  ${YEL}[1]${RST}  hot_counter.sh       ${DIM}Thermal truth companion${RST}\n"
    printf "  ${YEL}[2]${RST}  app_netwatch.sh      ${DIM}Per-app network monitor${RST}\n"
    printf "  ${YEL}[3]${RST}  dialer_spy.sh        ${DIM}Call event monitor${RST}\n"
    printf "  ${YEL}[4]${RST}  StalkerSlayer.sh     ${DIM}Telemetry blocker${RST}\n"
    printf "  ${YEL}[5]${RST}  kernel_sanders.sh    ${DIM}Kernel audit${RST}\n"
    printf "  ${YEL}[6]${RST}  bell_tower.sh        ${DIM}Alert monitor${RST}\n"
    printf "  ${YEL}[7]${RST}  freq_like_me.sh      ${DIM}CPU frequency viewer${RST}\n"
    printf "  ${YEL}[8]${RST}  multi_sensory_room.sh ${DIM}Sensor registry${RST}\n"
    printf "  ${YEL}[9]${RST}  ram_slammer_v2.sh    ${DIM}Memory + process${RST}\n"
    printf "  ${YEL}[b]${RST}  Back\n\n"
    read -r -p "$(printf "  ${CYN}Choice: ${RST}")" c
    case "$c" in
        1) "$TOOLS/hot_counter.sh"          ; section_tools ;;
        2) "$TOOLS/app_netwatch.sh"         ; section_tools ;;
        3) "$TOOLS/dialer_spy.sh"           ; section_tools ;;
        4) "$TOOLS/StalkerSlayer.sh"        ; section_tools ;;
        5) "$TOOLS/kernel_sanders.sh"       ; section_tools ;;
        6) "$TOOLS/bell_tower.sh"           ; section_tools ;;
        7) "$TOOLS/freq_like_me.sh"         ; section_tools ;;
        8) "$TOOLS/multi_sensory_room.sh"   ; section_tools ;;
        9) "$TOOLS/ram_slammer_v2.sh"       ; section_tools ;;
        b|B) main_menu ;;
        *) main_menu ;;
    esac
}

# ── Channel 6 ─────────────────────────────────────────────────────────────────
section_channel6() {
    "$TOOLS/april_oneil.sh"
    main_menu
}

# ── Live View ─────────────────────────────────────────────────────────────────
section_live() {
    clear
    masthead
    printf "  ${BOLD}${WHT}LIVE VIEW${RST}  ${DIM}Ctrl+C to stop${RST}\n\n"
    printf "  ${YEL}[1]${RST} Superhero live scan (continuous)\n"
    printf "  ${YEL}[2]${RST} Syndicate live feed\n"
    printf "  ${YEL}[3]${RST} Combined live view\n"
    printf "  ${YEL}[b]${RST} Back\n\n"
    read -r -p "$(printf "  ${CYN}Choice: ${RST}")" c
    case "$c" in
        1) live_superhero ;;
        2) live_syndicate ;;
        3) live_combined ;;
        b|B) main_menu ;;
        *) main_menu ;;
    esac
}

live_superhero() {
    clear
    printf "${BGBLUE}${WHT}${BOLD} SUPERHERO LIVE %-32s${RST}\n" "Ctrl+C to stop"
    printf "${CYN}%48s${RST}\n" | tr ' ' '='
    local cycles=$(get_superhero_cycles)
    local baseline=$(get_baseline_status)
    printf "  ${DIM}Cycles: %s/12  Baseline: %s${RST}\n\n" "$cycles" "$baseline"
    while true; do
        RISH_APPLICATION_ID=com.termux "$BIN/superhero" 2>&1 | \
            grep -E "LEO|DON|RAPH|MIKEY|CASEY|finding|WARN|INFO" | \
            while IFS= read -r line; do
                printf "  ${CYN}%s${RST}  ${WHT}%s${RST}\n" \
                    "$(date '+%H:%M:%S')" "$line"
            done
        printf "\n  ${DIM}── Cycle complete — restarting in 5s ──${RST}\n\n"
        sleep 5
    done
}

live_syndicate() {
    clear
    trap 'main_menu' INT
    printf "${BGBLUE}${WHT}${BOLD} SYNDICATE LIVE FEED %-27s${RST}\n" "Ctrl+C to stop"
    while true; do
        printf "\033[4;1H"  # Move to line 4
        local now=$(date '+%H:%M:%S')
        printf "${BGBLUE}${WHT}${BOLD} %-46s${RST}\n\n" "Updated: $now"
        for d in burned granitord leatherheadd metalheadd rahzerd ratkingd \
                  rocksteadyd shredderd tigerclawd bebopd fugitoidd nulld overlordd; do
            local f="$RESULTS/${d}.json"
            if [ -f "$f" ]; then
                local ts=$(jq -r '.timestamp // "?"' "$f" 2>/dev/null | \
                    grep -oE '[0-9]{2}:[0-9]{2}:[0-9]{2}' | head -1)
                local extra=""
                case "$d" in
                    rahzerd)     extra=$(jq -r '"wifi=\(.wifi.connected) dns=\(.dns.latency_ms)ms"' "$f" 2>/dev/null) ;;
                    tigerclawd)  extra=$(jq -r '"trust=\(.trust_score)/100"' "$f" 2>/dev/null) ;;
                    overlordd)   extra=$(jq -r '"threat=\(.threat_level) patterns=\(.pattern_count)"' "$f" 2>/dev/null) ;;
                    nulld)       extra=$(jq -r '"screen=\(.screen) spikes=\(.total_spike_events)"' "$f" 2>/dev/null) ;;
                    leatherheadd) extra=$(jq -r '"score=\(.thermal_score)/100"' "$f" 2>/dev/null) ;;
                    bebopd)      extra=$(jq -r '"drain=\(.drain_mah_h)mAh/hr"' "$f" 2>/dev/null) ;;
                    burned)      extra=$(jq -r '"signals=\(.privacy_signal_count)"' "$f" 2>/dev/null) ;;
                esac
                printf "  ${GRN}●${RST} ${CYN}%-14s${RST} ${DIM}%s${RST} ${WHT}%s${RST}          \n" \
                    "$d" "$ts" "$extra"
            else
                printf "  ${DIM}○ %-14s no data${RST}          \n" "$d"
            fi
        done
        sleep 5
    done
    trap - INT
}

live_combined() {
    clear
    trap 'main_menu' INT
    printf "${BGBLUE}${WHT}${BOLD} COMBINED LIVE VIEW %-28s${RST}\n" "Ctrl+C to stop"
    printf "${CYN}%48s${RST}\n\n" | tr ' ' '='

    # Launch superhero in background
    RISH_APPLICATION_ID=com.termux "$BIN/superhero" \
        >> "$BASE/logs/superhero_live.log" 2>&1 &
    local hero_pid=$!

    while true; do
        printf "\033[4;1H"
        local now=$(date '+%H:%M:%S')
        printf "  ${BOLD}${CYN}SUPERHERO${RST}  ${DIM}%s${RST}\n" "$now"
        tail -5 "$BASE/logs/superhero_live.log" 2>/dev/null | \
            grep -E "LEO|DON|RAPH|MIKEY|finding" | \
            while IFS= read -r line; do
                printf "  ${WHT}%s${RST}          \n" "${line:0:44}"
            done
        printf "\n  ${BOLD}${CYN}SYNDICATE${RST}\n"
        for d in rahzerd tigerclawd nulld overlordd; do
            local f="$RESULTS/${d}.json"
            if [ -f "$f" ]; then
                local extra=""
                case "$d" in
                    rahzerd)    extra=$(jq -r '"wifi=\(.wifi.connected) dns=\(.dns.latency_ms)ms ports=\(.ports.established_tcp4+.ports.established_tcp6)"' "$f" 2>/dev/null) ;;
                    tigerclawd) extra=$(jq -r '"trust=\(.trust_score)/100 drift=\(.binder.drift)"' "$f" 2>/dev/null) ;;
                    nulld)      extra=$(jq -r '"screen=\(.screen) spikes=\(.total_spike_events)"' "$f" 2>/dev/null) ;;
                    overlordd)  extra=$(jq -r '"THREAT=\(.threat_level) patterns=\(.pattern_count)"' "$f" 2>/dev/null) ;;
                esac
                printf "  ${GRN}●${RST} ${CYN}%-12s${RST} ${WHT}%s${RST}          \n" "$d" "$extra"
            fi
        done
        sleep 5
    done
    kill $hero_pid 2>/dev/null
    trap - INT
}

# ── Privacy & Data ────────────────────────────────────────────────────────────
section_privacy() {
    "$TOOLS/miuiser_sar.sh"
    main_menu
}

# ── Route choices ─────────────────────────────────────────────────────────────
handle_choice() {
    case "$1" in
        1) section_superhero ;;
        2) section_syndicate ;;
        3) section_tools ;;
        4) section_channel6 ;;
        5) section_live ;;
        6) section_privacy ;;
        q|Q) clear; exit 0 ;;
        *) main_menu ;;
    esac
}

# ── Entry point ───────────────────────────────────────────────────────────────
main_menu
