#!/data/data/com.termux/files/usr/bin/bash
# ==============================================================================
#  Multi-Sensory Room — Sensor Registry & Enforcement Companion
#  Companion to: metalheadd daemon
#  Theme: Neon everything, stacked quadrants (mobile-first)
#  Detect -> Investigate -> Enforce
# ==============================================================================

BASE="$HOME/MiuiserPeruser"
RESULTS="$BASE/Registry/daemon_results/metalheadd.json"
ADB="adb -s 127.0.0.1:5555"

# ── Neon palette ──────────────────────────────────────────────────────────────
N_PINK='\033[38;5;198m'
N_CYAN='\033[38;5;51m'
N_GREEN='\033[38;5;46m'
N_YELLOW='\033[38;5;226m'
N_PURPLE='\033[38;5;129m'
N_ORANGE='\033[38;5;208m'
N_LIME='\033[38;5;118m'
N_RED='\033[38;5;196m'
DIM='\033[38;5;237m'
WHITE='\033[38;5;255m'
BOLD='\033[1m'
RESET='\033[0m'

COLS=$(tput cols 2>/dev/null || echo 60)
W=$(( COLS - 4 ))
[ "$W" -gt 60 ] && W=60

# ── Single ADB fetch — all quads share this ───────────────────────────────────
SENSOR_DUMP=""
fetch_dump() {
    SENSOR_DUMP=$(printf '%s
' "$SENSOR_DUMP" | tr -d '
')
}

# ── Helpers ───────────────────────────────────────────────────────────────────
run_cmd() { adb -s 127.0.0.1:5555 shell "$*" 2>/dev/null; }

divider() { echo -e "${1}$(printf '%*s' "$COLS" | tr ' ' "${2:--}")${RESET}"; }

box_top() {
    local title=$1 color=$2
    local inner=$(( W - 2 ))
    local pad=$(( (inner - ${#title}) / 2 )); [ $pad -lt 0 ] && pad=0
    echo -e "${color}+$(printf '%*s' "$inner" | tr ' ' '-')+${RESET}"
    printf "${color}|${RESET}%${pad}s${BOLD}${WHITE}%s${RESET}%$(( inner - pad - ${#title} ))s${color}|${RESET}\n" "" "$title" ""
    echo -e "${color}+$(printf '%*s' "$inner" | tr ' ' '-')+${RESET}"
}
box_bot() { echo -e "${1}+$(printf '%*s' "$(( W - 2 ))" | tr ' ' '-')+${RESET}"; }
box_row() {
    local color=$1 content=$2 plain=$3
    local inner=$(( W - 2 ))
    [ -z "$plain" ] && plain="$content"
    local padlen=$(( inner - ${#plain} - 1 ))
    [ $padlen -lt 0 ] && padlen=0
    printf "${color}|${RESET} %b%${padlen}s${color}|${RESET}\n" "$content" ""
}

# ── Header ────────────────────────────────────────────────────────────────────
header() {
    clear
    divider "$N_PINK" "="
    local title="*  M U L T I - S E N S O R Y   R O O M  *"
    local sub="metalheadd daemon / detect-investigate-enforce"
    local tp=$(( (COLS - ${#title}) / 2 )); [ $tp -lt 0 ] && tp=0
    local sp=$(( (COLS - ${#sub}) / 2 ));   [ $sp -lt 0 ] && sp=0
    printf "%${tp}s${BOLD}${N_CYAN}%s${RESET}\n" "" "$title"
    printf "%${sp}s${DIM}%s${RESET}\n" "" "$sub"
    divider "$N_PURPLE" "="
    echo ""
}

# ── Q1: Last scan ─────────────────────────────────────────────────────────────
quad_scan() {
    box_top "LAST SCAN" "$N_PINK"
    if [ ! -f "$RESULTS" ]; then
        box_row "$N_PINK" "  No results yet — run metalheadd first" "  No results yet — run metalheadd first"
        box_bot "$N_PINK"; return
    fi
    local score grade ts total sensitive signals
    score=$(jq -r '.sensor_score // "?"' "$RESULTS" 2>/dev/null)
    grade=$(jq -r '.grade // "?"' "$RESULTS" 2>/dev/null)
    ts=$(jq -r '.timestamp // "?"' "$RESULTS" 2>/dev/null)
    total=$(jq -r '.total_sensors // 0' "$RESULTS" 2>/dev/null)
    sensitive=$(jq -r '.sensitive_active // 0' "$RESULTS" 2>/dev/null)
    signals=$(jq -r '.signals_fired // 0' "$RESULTS" 2>/dev/null)
    box_row "$N_PINK" "  Score    : ${N_YELLOW}${score}/100 [${grade}]${RESET}" "  Score    : ${score}/100 [${grade}]"
    box_row "$N_PINK" "  Time     : ${DIM}${ts}${RESET}" "  Time     : ${ts}"
    box_row "$N_PINK" "  Sensors  : ${N_CYAN}${total}${RESET} registered" "  Sensors  : ${total} registered"
    box_row "$N_PINK" "  Sensitive: ${N_ORANGE}${sensitive}${RESET} active" "  Sensitive: ${sensitive} active"
    box_row "$N_PINK" "  Signals  : ${N_YELLOW}${signals}${RESET} fired" "  Signals  : ${signals} fired"
    box_bot "$N_PINK"
}

# ── Q2: Sensor types from Sensor List ─────────────────────────────────────────
quad_types() {
    box_top "SENSOR TYPES" "$N_CYAN"
    local raw found=0 name
    raw=$(printf '%s
' "$SENSOR_DUMP")
    local filtered
    filtered=$(printf '%s
' "$raw" | grep '0x' | grep '|' | head -8)
    if [ -z "$filtered" ]; then
        box_row "$N_CYAN" "  Unavailable" "  Unavailable"
        box_bot "$N_CYAN"; return
    fi
    while IFS= read -r line; do
        name=$(printf '%s' "$line" | tr -d '
' | sed 's/.*) //' | cut -d'|' -f1 | sed 's/[[:space:]]*$//')
        [ -n "$name" ] && box_row "$N_CYAN" "  ${N_LIME}o${RESET} ${name}" "  o ${name}" && found=$((found+1))
    done <<< "$filtered"
    [ $found -eq 0 ] && box_row "$N_CYAN" "  Unavailable" "  Unavailable"
    box_bot "$N_CYAN"
}

# ── Q3: Active sensitive sensors ──────────────────────────────────────────────
quad_sensitive() {
    box_top "SENSITIVE SENSORS" "$N_ORANGE"
    local found=0
    local line name conns
    while IFS= read -r line; do
        name=$(printf '%s' "$line" | awk '{print $1}')
        conns=$(printf '%s' "$line" | grep -o 'connections=[0-9]*' | grep -o '[0-9]*')
        for stype in ACCELEROMETER GYROSCOPE MAGNETOMETER LINEARACCEL ROTATION_VECTOR SIGNIFICANT_MOTION STEP_DETECTOR; do
            if printf '%s' "$name" | grep -qi "$stype"; then
                box_row "$N_ORANGE" "  ${N_YELLOW}!${RESET} ${name} (${conns:-?} conn)" "  ! ${name} (${conns:-?} conn)"
                found=$((found+1))
            fi
        done
    done < <(printf '%s
' "$SENSOR_DUMP" | tr -d '\r' | awk '/^Active sensors:/,/^Socket Buffer/' | grep -v "^Active\|^Socket\|^$")
    [ $found -eq 0 ] && box_row "$N_ORANGE" "  ${N_GREEN}OK${RESET} None flagged" "  OK None flagged"
    box_bot "$N_ORANGE"
}

# ── Q4: Live connections ───────────────────────────────────────────────────────
quad_connections() {
    box_top "ACTIVE CONNECTIONS" "$N_PURPLE"
    local found=0
    local line pkg uid
    while IFS= read -r line; do
        pkg=$(printf '%s' "$line" | awk -F'|' '{print $1}' | sed 's/^ *//;s/ *$//' | rev | cut -d. -f1 | rev | cut -c1-$(( W - 14 )))
        uid=$(printf '%s' "$line" | grep -o 'uid [0-9]*' | grep -o '[0-9]*')
        [ -z "$pkg" ] && continue
        local col="$N_GREEN"
        [ "${uid:-0}" -gt 9999 ] 2>/dev/null && col="$N_PINK"
        box_row "$N_PURPLE" "  ${col}u${uid}${RESET} ${pkg}" "  u${uid} ${pkg}"
        found=$((found+1))
    done < <(printf '%s
' "$SENSOR_DUMP" | tr -d '\r' | grep "| uid")
    [ $found -eq 0 ] && box_row "$N_PURPLE" "  No active connections" "  No active connections"
    box_bot "$N_PURPLE"
}

# ── Full sensor list ───────────────────────────────────────────────────────────
full_sensor_list() {
    header
    divider "$N_GREEN" "-"
    echo -e "${BOLD}${N_GREEN}  ALL REGISTERED SENSORS${RESET}"
    divider "$N_GREEN" "-"
    echo ""
    local line name vendor typ
    while IFS= read -r line; do
        name=$(printf '%s' "$line" | sed 's/.*) //' | cut -d'|' -f1 | sed 's/[[:space:]]*$//')
        vendor=$(printf '%s' "$line" | cut -d'|' -f2 | sed 's/^ *//;s/ *$//')
        typ=$(printf '%s' "$line" | grep -o 'type:[^(]*(' | sed 's/type: *//;s/(//')
        [ -n "$name" ] && printf "  ${N_CYAN}%-26.26s${RESET} ${N_YELLOW}%-22.22s${RESET} ${DIM}%s${RESET}\n" "$name" "$typ" "$vendor"
    done < <(printf '%s
' "$SENSOR_DUMP" | tr -d '\r' | grep -E "0x[0-9a-fA-F]+\)" | grep "|")
    echo ""
}

# ── Live connections detail (for investigate) ──────────────────────────────────
show_live_connections() {
    echo -e "${N_GREEN}Current sensor clients:${RESET}"
    echo ""
    local line pkg uid
    while IFS= read -r line; do
        pkg=$(printf '%s' "$line" | awk -F'|' '{print $1}' | sed 's/^ *//;s/ *$//')
        uid=$(printf '%s' "$line" | grep -o 'uid [0-9]*' | grep -o '[0-9]*')
        [ -z "$pkg" ] && continue
        local col="$N_GREEN"
        [ "${uid:-0}" -gt 9999 ] 2>/dev/null && col="$N_PINK"
        printf "  ${col}uid=%-6s${RESET} ${WHITE}%s${RESET}\n" "$uid" "$pkg"
    done < <(printf '%s
' "$SENSOR_DUMP" | tr -d '\r' | grep "| uid")
    echo ""
}

# ── Investigate + enforce ──────────────────────────────────────────────────────
investigate() {
    header
    show_live_connections
    divider "$N_PURPLE" "-"
    echo -e "${N_YELLOW}Enter package name to investigate:${RESET}"
    read -r -p "$(echo -e "${N_PURPLE}> ${RESET}")" pkg
    [ -z "$pkg" ] && return

    local base_pkg
    base_pkg=$(printf '%s' "$pkg" | cut -d'$' -f1)

    header
    echo -e "${N_CYAN}-- Investigating: ${N_YELLOW}${pkg}${RESET}"
    echo ""

    echo -e "${N_GREEN}Sensor/location permissions (granted):${RESET}"
    run_cmd "dumpsys package $base_pkg 2>/dev/null | grep -E 'SENSOR|LOCATION|BODY|ACTIVITY_RECOGNITION' | grep granted=true" | \
    while IFS= read -r line; do
        perm=$(printf '%s' "$line" | grep -o 'android.permission.[A-Z_]*')
        [ -n "$perm" ] && echo -e "  ${N_ORANGE}! ${perm}${RESET}"
    done
    echo ""

    echo -e "${N_GREEN}Active sensor connections:${RESET}"
    printf '%s
' "$SENSOR_DUMP" | tr -d '\r' | grep "$pkg" | head -6 | \
    while IFS= read -r line; do
        echo -e "  ${N_PINK}${line}${RESET}" | cut -c1-${COLS}
    done
    echo ""

    divider "$N_CYAN" "-"
    echo -e "  ${N_ORANGE}[1]${RESET} Revoke BODY_SENSORS"
    echo -e "  ${N_RED}[2]${RESET} Revoke HIGH_SAMPLING_RATE_SENSORS"
    echo -e "  ${N_YELLOW}[3]${RESET} Revoke ACTIVITY_RECOGNITION"
    echo -e "  ${N_PINK}[4]${RESET} Kill process"
    echo -e "  ${DIM}[q]${RESET} Back"
    echo ""
    read -r -p "$(echo -e "${N_PURPLE}Choice: ${RESET}")" action

    case "$action" in
        1) run_cmd "appops set $base_pkg BODY_SENSORS deny"
           echo -e "${N_GREEN}BODY_SENSORS denied${RESET}" ;;
        2) run_cmd "appops set $base_pkg HIGH_SAMPLING_RATE_SENSORS deny"
           echo -e "${N_GREEN}HIGH_SAMPLING_RATE_SENSORS denied${RESET}" ;;
        3) run_cmd "appops set $base_pkg ACTIVITY_RECOGNITION deny"
           echo -e "${N_GREEN}ACTIVITY_RECOGNITION denied${RESET}" ;;
        4) local pid
           pid=$(run_cmd "pidof $base_pkg" | tr -d '\r')
           if [ -n "$pid" ]; then
               run_cmd "kill $pid"
               echo -e "${N_GREEN}Killed pid ${pid}${RESET}"
           else
               echo -e "${N_ORANGE}Process not found${RESET}"
           fi ;;
        q|Q) return ;;
    esac
    sleep 2
}

# ── Watch mode ─────────────────────────────────────────────────────────────────
watch_mode() {
    while true; do
        header
        fetch_dump
        quad_scan;        echo ""
        quad_types;       echo ""
        quad_sensitive;   echo ""
        quad_connections; echo ""
        divider "$N_PINK" "-"
        echo -e "${DIM}  Refreshing in 5s — Ctrl+C to stop${RESET}"
        sleep 5
    done
}

# ── Rescan ─────────────────────────────────────────────────────────────────────
rescan() {
    echo -e "${N_YELLOW}Triggering metalheadd rescan...${RESET}"
    pkill -x metalheadd 2>/dev/null
    sleep 1
    "$BASE/bin/metalheadd" >> "$BASE/logs/metalheadd.log" 2>&1 &
    echo -e "${N_GREEN}Done — results in ~2s${RESET}"
    sleep 3
}

# ── Menu ───────────────────────────────────────────────────────────────────────
menu() {
    divider "$N_CYAN" "-"
    echo ""
    echo -e "  ${N_PINK}[1]${RESET} All quadrants"
    echo -e "  ${N_CYAN}[2]${RESET} Full sensor list"
    echo -e "  ${N_GREEN}[3]${RESET} Watch mode (5s refresh)"
    echo -e "  ${N_ORANGE}[4]${RESET} Force metalheadd rescan"
    echo -e "  ${N_YELLOW}[5]${RESET} Investigate + enforce"
    echo -e "  ${DIM}[q]${RESET} Quit"
    echo ""
    read -r -p "$(echo -e "${N_PURPLE}Choice: ${RESET}")" choice

    case "$choice" in
        1) header; quad_scan; echo ""; quad_types; echo ""; quad_sensitive; echo ""; quad_connections; echo ""; menu ;;
        2) full_sensor_list; menu ;;
        3) watch_mode ;;
        4) rescan; menu ;;
        5) investigate; header; quad_scan; echo ""; quad_connections; echo ""; menu ;;
        q|Q) exit 0 ;;
        *) echo -e "${N_PINK}Invalid${RESET}"; sleep 1; menu ;;
    esac
}

# ── Main ───────────────────────────────────────────────────────────────────────
header
fetch_dump
quad_scan
echo ""
quad_types
echo ""
quad_sensitive
echo ""
quad_connections
echo ""
menu
