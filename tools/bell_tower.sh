#!/data/data/com.termux/files/usr/bin/bash
source "$(dirname "$0")/lib/tool_backend.sh"
# ==============================================================================
#  The Bell Tower — Power & Wakelock Companion
#  Companion to: bebopd daemon
#  Theme: 1980s digital alarm clock — red LED 7-segment, silver chrome
#  Detect -> Investigate -> Enforce
# ==============================================================================

BASE="$HOME/MiuiserPeruser"
RESULTS="$BASE/Registry/daemon_results/bebopd.json"
ADB="adb -s 127.0.0.1:5555"

# ── Palette ───────────────────────────────────────────────────────────────────
LED='\033[38;5;196m'
LED2='\033[38;5;88m'
CHROME='\033[38;5;251m'
CHROME2='\033[38;5;245m'
AMBER='\033[38;5;214m'
DIM='\033[38;5;238m'
WHITE='\033[38;5;255m'
BOLD='\033[1m'
RESET='\033[0m'

COLS=$(tput cols 2>/dev/null || echo 64)
run_cmd() { $ADB shell "$*" 2>/dev/null; }

# ── 7-segment display ─────────────────────────────────────────────────────────
# Each digit rendered as 3 rows
# Row 1: top bar
# Row 2: middle  
# Row 3: bottom
#
#  _
# | |   0
#  _
# |_|   0 again bottom
#
# Segments per digit:
#  _
# |_|  = 8 (all)
# |_|

seg_r1() {
    case "$1" in
        0|2|3|5|6|7|8|9) printf " _ " ;;
        1|4)              printf "   " ;;
        :)                printf "   " ;;
        *)                printf "   " ;;
    esac
}

seg_r2() {
    case "$1" in
        0)  printf "| |" ;;
        1)  printf "  |" ;;
        2)  printf " _|" ;;
        3)  printf " _|" ;;
        4)  printf "|_|" ;;
        5)  printf "|_ " ;;
        6)  printf "|_ " ;;
        7)  printf "  |" ;;
        8)  printf "|_|" ;;
        9)  printf "|_|" ;;
        :)  printf "   " ;;
        *)  printf "   " ;;
    esac
}

seg_r3() {
    case "$1" in
        0)  printf "|_|" ;;
        1)  printf "  |" ;;
        2)  printf "|_ " ;;
        3)  printf " _|" ;;
        4)  printf "  |" ;;
        5)  printf " _|" ;;
        6)  printf "|_|" ;;
        7)  printf "  |" ;;
        8)  printf "|_|" ;;
        9)  printf " _|" ;;
        :)  printf " * " ;;
        *)  printf "   " ;;
    esac
}

led_display() {
    local t; t=$(date "+%H:%M:%S")
    # Build array: H H : M M : S S
    local chars=()
    for i in $(seq 0 $((${#t}-1))); do
        chars+=("${t:$i:1}")
    done

    local pad=$(( (COLS - ${#chars[@]} * 4) / 2 ))
    [ $pad -lt 0 ] && pad=0

    # Row 1
    printf "%${pad}s" ""
    for c in "${chars[@]}"; do
        printf "${LED}$(seg_r1 "$c")${RESET} "
    done
    echo ""

    # Row 2
    printf "%${pad}s" ""
    for c in "${chars[@]}"; do
        if [ "$c" = ":" ]; then
            printf "${CHROME}$(seg_r2 "$c")${RESET} "
        else
            printf "${LED}$(seg_r2 "$c")${RESET} "
        fi
    done
    echo ""

    # Row 3
    printf "%${pad}s" ""
    for c in "${chars[@]}"; do
        if [ "$c" = ":" ]; then
            printf "${CHROME}$(seg_r3 "$c")${RESET} "
        else
            printf "${LED}$(seg_r3 "$c")${RESET} "
        fi
    done
    echo ""
}

# ── Dividers ──────────────────────────────────────────────────────────────────
clock_rule() { echo -e "${CHROME}$(printf '%*s' "$COLS" | tr ' ' '-')${RESET}"; }
tick_rule() {
    local line="" i=0
    while [ $i -lt $COLS ]; do
        [ $(( i % 4 )) -eq 0 ] && line="${line}${CHROME}|${RESET}" || line="${line}${CHROME2}-${RESET}"
        i=$((i+1))
    done
    echo -e "$line"
}

# ── Header ────────────────────────────────────────────────────────────────────
header() {
    clear
    tick_rule
    local title="T H E   B E L L   T O W E R"
    local sub="Power + Wakelock Companion  /  bebopd daemon"
    local tp=$(( (COLS - ${#title}) / 2 )); [ $tp -lt 0 ] && tp=0
    local sp=$(( (COLS - ${#sub}) / 2 )); [ $sp -lt 0 ] && sp=0
    echo ""
    printf "%${tp}s${BOLD}${LED}%s${RESET}\n" "" "$title"
    printf "%${sp}s${CHROME2}%s${RESET}\n" "" "$sub"
    echo ""
    led_display
    echo ""
    tick_rule
    echo ""
}

# ── Last scan ─────────────────────────────────────────────────────────────────
show_last_scan() {
    clock_rule
    echo -e "  ${BOLD}${CHROME}LAST BEBOPD SCAN${RESET}"
    clock_rule

    if [ ! -f "$RESULTS" ]; then
        echo -e "  ${LED2}No results yet${RESET}"; echo ""; return
    fi

    local score grade ts drain level wakelocks nonsys
    score=$(jq -r '.power_score // "?"' "$RESULTS" 2>/dev/null)
    grade=$(jq -r '.grade // "?"' "$RESULTS" 2>/dev/null)
    ts=$(jq -r '.timestamp // "?"' "$RESULTS" 2>/dev/null)
    drain=$(jq -r '.drain_mah_h // 0' "$RESULTS" 2>/dev/null)
    level=$(jq -r '.battery_pct // "?"' "$RESULTS" 2>/dev/null)
    wakelocks=$(jq -r '.active_wakelocks // 0' "$RESULTS" 2>/dev/null)
    nonsys=$(jq -r '.full_nonsystem_wakelocks // 0' "$RESULTS" 2>/dev/null)

    local sc="$CHROME"
    case "$grade" in ACTIVE) sc="$AMBER";; DRAINING|CRITICAL) sc="$LED";; esac

    printf "  ${CHROME2}Score    :${RESET} ${sc}${BOLD}%s/100  [%s]${RESET}\n" "$score" "$grade"
    printf "  ${CHROME2}Scanned  :${RESET} ${DIM}%s${RESET}\n" "$ts"
    printf "  ${CHROME2}Battery  :${RESET} ${WHITE}%s%%${RESET}\n" "$level"

    local drain_int=${drain%.*}
    local dc="$CHROME"
    [ "${drain_int:-0}" -gt 500 ]  && dc="$AMBER"
    [ "${drain_int:-0}" -gt 1000 ] && dc="$LED"
    printf "  ${CHROME2}Drain    :${RESET} ${dc}${BOLD}%s mAh${RESET} ${CHROME2}(computed total)${RESET}\n" "$drain"
    printf "  ${CHROME2}Wakelocks:${RESET} ${WHITE}%s${RESET} active  ${LED}%s${RESET} non-system\n" "$wakelocks" "$nonsys"
    echo ""
}

# ── Live battery ──────────────────────────────────────────────────────────────
show_live_battery() {
    clock_rule
    echo -e "  ${BOLD}${CHROME}LIVE BATTERY${RESET}"
    clock_rule

    local raw; raw=$(run_cmd "dumpsys battery")
    local level voltage temp status
    level=$(echo "$raw"   | grep "level:"       | awk '{print $2}' | tr -d '\r')
    voltage=$(echo "$raw" | grep "^  voltage:"  | awk '{print $2}' | tr -d '\r')
    temp=$(echo "$raw"    | grep "temperature:" | awk '{print $2}' | tr -d '\r')
    local status_raw; status_raw=$(echo "$raw" | grep "status:" | awk '{print $2}' | tr -d '
')
    case "$status_raw" in
        1) status="Unknown" ;; 2) status="Charging" ;;
        3) status="Discharging" ;; 4) status="Not charging" ;;
        5) status="Full" ;; *) status="$status_raw" ;;
    esac

    # Battery bar using | chars like LCD
    local bar_w=28 filled=$(( ${level:-0} * 28 / 100 ))
    local bar="" i=0
    while [ $i -lt $filled ]; do
        local pct=$(( i * 100 / bar_w ))
        if   [ $pct -ge 60 ]; then bar="${bar}${CHROME}|${RESET}"
        elif [ $pct -ge 30 ]; then bar="${bar}${AMBER}|${RESET}"
        else                        bar="${bar}${LED}|${RESET}"
        fi
        i=$((i+1))
    done
    i=0; while [ $i -lt $(( bar_w - filled )) ]; do bar="${bar}${DIM}.${RESET}"; i=$((i+1)); done

    printf "  ${CHROME2}Level  :${RESET} [%b] ${WHITE}%s%%${RESET}\n" "$bar" "${level:-?}"

    local temp_c=""; [ -n "$temp" ] && temp_c=$(echo "$temp" | awk '{printf "%.1f", $1/10}')
    local tc="$CHROME"; [ "${temp:-0}" -gt 350 ] && tc="$AMBER"; [ "${temp:-0}" -gt 450 ] && tc="$LED"
    printf "  ${CHROME2}Voltage:${RESET} ${CHROME}%s mV${RESET}   ${CHROME2}Temp:${RESET} ${tc}%s°C${RESET}\n" "${voltage:-?}" "$temp_c"
    printf "  ${CHROME2}Status :${RESET} ${WHITE}%s${RESET}\n" "${status:-?}"
    echo ""
}

# ── Live wakelocks ────────────────────────────────────────────────────────────
show_live_wakelocks() {
    clock_rule
    echo -e "  ${BOLD}${CHROME}LIVE KERNEL WAKELOCKS${RESET}"
    clock_rule

    local out
    out=$(run_cmd "dumpsys batterystats | grep 'Kernel Wake lock' | head -10")
    if [ -z "$out" ]; then
        echo -e "  ${CHROME2}No kernel wakelocks${RESET}"; echo ""; return
    fi

    echo "$out" | tr -d '\r' | while IFS= read -r line; do
        local name duration
        name=$(echo "$line" | grep -o 'Wake lock [^:]*:' | sed 's/Wake lock //;s/://')
        duration=$(echo "$line" | grep -o '[0-9]*m [0-9]*s [0-9]*ms\|[0-9]*h [0-9]*m')
        local col="$CHROME2"
        # Flag suspicious names
        echo "$name" | grep -qiE "wlan|wifi|modem|ccci|mt6" && col="$AMBER"
        printf "  ${col}%-32.32s${RESET} ${DIM}%s${RESET}\n" "${name:-$line}" "$duration"
    done
    echo ""
}

# ── Drain summary ─────────────────────────────────────────────────────────────
show_drain() {
    clock_rule
    echo -e "  ${BOLD}${CHROME}DRAIN SUMMARY${RESET}"
    clock_rule

    local out
    out=$(run_cmd "dumpsys batterystats | grep -E 'Computed drain|Screen off discharge|Estimated battery capacity|Estimated discharge time|Estimated screen on'")
    if [ -z "$out" ]; then
        echo -e "  ${LED2}Drain data unavailable${RESET}"; echo ""; return
    fi

    echo "$out" | tr -d '\r' | while IFS= read -r line; do
        echo -e "  ${CHROME2}${line}${RESET}"
    done
    echo ""
}

# ── Investigate + enforce ─────────────────────────────────────────────────────
investigate() {
    header
    clock_rule
    echo -e "  ${BOLD}${CHROME}LIVE WAKELOCK CLIENTS${RESET}"
    clock_rule
    echo ""

    # Show apps with wakelocks from dumpsys power
    local out
    out=$(run_cmd "dumpsys power | grep -A2 'Wake Locks:' | head -30")
    local wl_out
    wl_out=$(run_cmd "dumpsys power | grep -E 'WAKE_LOCK|ACQ=' | head -20")

    if [ -n "$wl_out" ]; then
        echo "$wl_out" | tr -d '\r' | while IFS= read -r line; do
            local col="$CHROME2"
            echo "$line" | grep -q "uid=[1-9][0-9][0-9][0-9][0-9]" && col="$LED"
            echo -e "  ${col}${line}${RESET}" | cut -c1-$COLS
        done
    else
        echo -e "  ${CHROME2}No active wakelocks${RESET}"
    fi

    # Also show top battery consumers
    echo ""
    clock_rule
    echo -e "  ${BOLD}${CHROME}TOP BATTERY CONSUMERS${RESET}"
    clock_rule
    run_cmd "dumpsys batterystats | grep -E '^    u0a|^    u0' | grep -v 'wake_lock' | head -10" | tr -d '\r' | \
    while IFS= read -r line; do
        local col="$CHROME2"
        echo -e "  ${col}${line}${RESET}" | cut -c1-$COLS
    done
    echo ""

    clock_rule
    echo -e "  ${BOLD}${CHROME}ENFORCE${RESET}"
    clock_rule
    echo ""
    echo -e "  ${LED}[1]${RESET} Force doze mode now"
    echo -e "  ${AMBER}[2]${RESET} Restrict app background battery"
    echo -e "  ${CHROME}[3]${RESET} Reset battery stats baseline"
    echo -e "  ${CHROME2}[4]${RESET} Kill a specific process"
    echo -e "  ${DIM}[q]${RESET} Back"
    echo ""
    read -r -p "$(echo -e "  ${LED}>> ${RESET}")" action

    case "$action" in
        1)
            run_cmd "dumpsys deviceidle force-idle"
            echo -e "  ${AMBER}Doze mode forced${RESET}"
            sleep 2 ;;
        2)
            echo -e "  ${CHROME2}Enter package name:${RESET}"
            read -r -p "  >> " pkg
            [ -n "$pkg" ] && run_cmd "appops set $pkg RUN_IN_BACKGROUND deny" && \
                echo -e "  ${CHROME}Background restricted for ${pkg}${RESET}" || \
                echo -e "  ${LED2}Failed${RESET}"
            sleep 2 ;;
        3)
            run_cmd "dumpsys batterystats --reset"
            echo -e "  ${CHROME}Battery stats reset${RESET}"
            sleep 2 ;;
        4)
            echo -e "  ${CHROME2}Enter package or process name:${RESET}"
            read -r -p "  >> " proc
            if [ -n "$proc" ]; then
                local pid; pid=$(run_cmd "pidof $proc" | tr -d '\r')
                [ -n "$pid" ] && run_cmd "kill $pid" && \
                    echo -e "  ${CHROME}Killed pid $pid${RESET}" || \
                    echo -e "  ${LED2}Process not found${RESET}"
            fi
            sleep 2 ;;
        q|Q) return ;;
    esac
}

# ── Watch mode ────────────────────────────────────────────────────────────────
watch_mode() {
    while true; do
        header
        show_last_scan
        show_live_battery
        clock_rule
        echo -e "  ${DIM}Refreshing in 10s — Ctrl+C to stop${RESET}"
        sleep 10
    done
}

# ── Rescan ────────────────────────────────────────────────────────────────────
rescan() {
    echo -e "${AMBER}Triggering bebopd rescan...${RESET}"
    pkill -x bebopd 2>/dev/null; sleep 1
    "$BASE/bin/bebopd" >> "$BASE/logs/bebopd.log" 2>&1 &
    echo -e "${CHROME}Done — results in ~10s${RESET}"; sleep 10
}

# ── Menu ──────────────────────────────────────────────────────────────────────
menu() {
    tick_rule; echo ""
    echo -e "  ${LED}[1]${RESET} Last bebopd scan"
    echo -e "  ${CHROME}[2]${RESET} Live battery"
    echo -e "  ${CHROME2}[3]${RESET} Kernel wakelocks"
    echo -e "  ${AMBER}[4]${RESET} Drain summary"
    echo -e "  ${LED}[5]${RESET} Investigate + enforce"
    echo -e "  ${CHROME}[6]${RESET} Watch mode (10s)"
    echo -e "  ${CHROME2}[7]${RESET} Force rescan"
    echo -e "  ${DIM}[q]${RESET} Quit"
    echo ""
    read -r -p "$(echo -e "  ${LED}>> ${RESET}")" choice
    case "$choice" in
        1) header; show_last_scan; menu ;;
        2) header; show_live_battery; menu ;;
        3) header; show_live_wakelocks; menu ;;
        4) header; show_drain; menu ;;
        5) investigate; menu ;;
        6) watch_mode ;;
        7) rescan; menu ;;
        q|Q) exit 0 ;;
        *) echo -e "  ${LED2}Invalid${RESET}"; sleep 1; menu ;;
    esac
}

# ── Main ──────────────────────────────────────────────────────────────────────
header
show_last_scan
menu
