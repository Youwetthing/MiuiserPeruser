#!/data/data/com.termux/files/usr/bin/bash
source "$(dirname "$0")/lib/tool_backend.sh"
# ==============================================================================
#  Hot Counter — Thermal Truth Companion
#  Companion to: leatherheadd daemon
#  Reads: Registry/daemon_results/leatherheadd.json
# ==============================================================================

BASE="$HOME/MiuiserPeruser"
RESULTS="$BASE/Registry/daemon_results/leatherheadd.json"
RISH="$HOME/rish"

# ── Colour palette — full heat spectrum ───────────────────────────────────────
R0='\033[38;5;196m'   # pure red
R1='\033[38;5;202m'   # red-orange
R2='\033[38;5;208m'   # orange
R3='\033[38;5;214m'   # amber
R4='\033[38;5;220m'   # golden
DIM='\033[38;5;238m'  # dark ember
GRAY='\033[38;5;245m'
WHITE='\033[38;5;255m'
BOLD='\033[1m'
RESET='\033[0m'

rish_cmd() {
    if "$RISH" -c "echo ok" 2>/dev/null | grep -q ok; then
        "$RISH" -c "$*" 2>/dev/null
    else
        adb -s 127.0.0.1:5555 shell "$*" 2>/dev/null
    fi
}

# ── Heat colour by temperature ────────────────────────────────────────────────
heat_color() {
    local t=${1%.*}
    if   [ "${t:-0}" -ge 50 ] 2>/dev/null; then echo -n "$R0"
    elif [ "${t:-0}" -ge 45 ] 2>/dev/null; then echo -n "$R1"
    elif [ "${t:-0}" -ge 40 ] 2>/dev/null; then echo -n "$R2"
    elif [ "${t:-0}" -ge 35 ] 2>/dev/null; then echo -n "$R3"
    else echo -n "$R4"
    fi
}

# ── Heat bar ──────────────────────────────────────────────────────────────────
heat_bar() {
    local val=$1 max=$2 width=30
    local filled=$(( val * width / max ))
    [ $filled -gt $width ] && filled=$width
    local empty=$(( width - filled ))
    local bar=""
    local i=0
    while [ $i -lt $filled ]; do
        pct=$(( i * 100 / width ))
        if   [ $pct -ge 80 ]; then bar="${bar}${R0}█${RESET}"
        elif [ $pct -ge 60 ]; then bar="${bar}${R1}█${RESET}"
        elif [ $pct -ge 40 ]; then bar="${bar}${R2}█${RESET}"
        elif [ $pct -ge 20 ]; then bar="${bar}${R3}█${RESET}"
        else                       bar="${bar}${R4}█${RESET}"
        fi
        i=$((i+1))
    done
    i=0
    while [ $i -lt $empty ]; do
        bar="${bar}${DIM}░${RESET}"
        i=$((i+1))
    done
    echo -n "$bar"
}

# ── Header ────────────────────────────────────────────────────────────────────
header() {
    clear
    echo -e "${R0}╔══════════════════════════════════════════════════════════════════╗${RESET}"
    echo -e "${R1}║${RESET}  ${BOLD}${R2}🔥  H O T   C O U N T E R${RESET}${R1}                                      ║${RESET}"
    echo -e "${R2}║${RESET}  ${GRAY}Thermal truth companion · leatherheadd daemon${RESET}${R2}                  ║${RESET}"
    echo -e "${R3}╚══════════════════════════════════════════════════════════════════╝${RESET}"
    echo ""
}

# ── Last daemon scan ──────────────────────────────────────────────────────────
show_last_scan() {
    echo -e "${R1}── Last Daemon Scan ──────────────────────────────────────────────${RESET}"

    if [ ! -f "$RESULTS" ]; then
        echo -e "${DIM}  No results yet — leatherheadd hasn't completed a scan${RESET}"
        echo ""
        return
    fi

    score=$(jq -r '.thermal_score // "?"' "$RESULTS" 2>/dev/null)
    grade=$(jq -r '.grade // "?"' "$RESULTS" 2>/dev/null)
    ts=$(jq -r '.timestamp // "?"' "$RESULTS" 2>/dev/null)
    throttled=$(jq -r '.throttled_cores // 0' "$RESULTS" 2>/dev/null)
    total=$(jq -r '.total_cores // 0' "$RESULTS" 2>/dev/null)
    signals=$(jq -r '.signals_fired // 0' "$RESULTS" 2>/dev/null)

    case "$grade" in
        NOMINAL)  sc="$R4" ;;
        WARM)     sc="$R3" ;;
        HOT)      sc="$R1" ;;
        CRITICAL) sc="$R0" ;;
        *)        sc="$GRAY" ;;
    esac

    echo -e "  ${GRAY}Score    :${RESET} ${sc}${BOLD}${score}/100  [${grade}]${RESET}"
    echo -e "  ${GRAY}Scanned  :${RESET} ${DIM}${ts}${RESET}"
    echo -e "  ${GRAY}Signals  :${RESET} ${R2}${signals}${RESET}"
    echo -e "  ${GRAY}Throttled:${RESET} ${R1}${throttled}${RESET}${GRAY}/${total} cores${RESET}"
    echo ""

    echo -e "${R2}── Sensor Readings ───────────────────────────────────────────────${RESET}"
    printf "  ${GRAY}%-14s  %8s  %8s  %8s${RESET}\n" "Sensor" "Cached°C" "HAL°C" "Delta"
    echo -e "  ${DIM}──────────────────────────────────────────────${RESET}"

    jq -c '.temperatures[]' "$RESULTS" 2>/dev/null | while read -r entry; do
        name=$(echo "$entry" | jq -r '.name')
        cached=$(echo "$entry" | jq -r '.cached')
        hal=$(echo "$entry"   | jq -r '.hal')
        delta=$(echo "$entry" | jq -r '.delta')

        col=$(heat_color "${hal:-$cached}")
        flag=""
        abs_delta=${delta#-}
        [ "${abs_delta%.*}" -ge 5 ] 2>/dev/null && flag="  ${R0}⚠ MISMATCH${RESET}"

        printf "  ${WHITE}%-14s${RESET}  ${col}%8s${RESET}  ${col}%8s${RESET}  %+8s%s\n" \
               "$name" "$cached" "$hal" "$delta" "$flag"
    done
    echo ""
}

# ── Live CPU frequencies ──────────────────────────────────────────────────────
live_cpu() {
    echo -e "${R2}── CPU Frequencies ───────────────────────────────────────────────${RESET}"
    printf "  ${GRAY}%-6s  %-8s  %-8s  %-4s  %s${RESET}\n" \
           "Core" "Cur MHz" "Max MHz" "Pct" "Heat"
    echo -e "  ${DIM}──────────────────────────────────────────────────────${RESET}"

    for i in 0 1 2 3 4 5 6 7; do
        cur=$(cat "/sys/devices/system/cpu/cpu${i}/cpufreq/scaling_cur_freq" 2>/dev/null)
        max=$(cat "/sys/devices/system/cpu/cpu${i}/cpufreq/cpuinfo_max_freq" 2>/dev/null)
        min=$(cat "/sys/devices/system/cpu/cpu${i}/cpufreq/cpuinfo_min_freq" 2>/dev/null)
        [ -z "$cur" ] && break

        mhz_cur=$((cur / 1000))
        mhz_max=$((max / 1000))
        pct=$((cur * 100 / max))

        # Cluster detection by min_freq
        if [ "${min:-0}" -le 600000 ] 2>/dev/null; then
            cluster="${GRAY}[E]${RESET}"
        else
            cluster="${R2}[P]${RESET}"
        fi

        bar=$(heat_bar $pct 100)
        flag=""
        [ "$pct" -lt 75 ] && flag=" ${R3}throttled${RESET}"
        [ "$pct" -ge 100 ] && flag=" ${R0}MAX${RESET}"

        printf "  %s cpu%-2d  %-8d  %-8d  %3d%%  %b%b\n" \
               "$(echo -e $cluster)" "$i" "$mhz_cur" "$mhz_max" "$pct" "$bar" "$flag"
    done
    echo ""
}

# ── Live HAL temps ────────────────────────────────────────────────────────────
live_hal() {
    echo -e "${R1}── Live HAL Temperatures ─────────────────────────────────────────${RESET}"
    local out
    out=$(rish_cmd "dumpsys thermalservice 2>/dev/null | grep 'Temperature{'" | head -12)

    if [ -z "$out" ]; then
        echo -e "${DIM}  HAL unavailable — showing cached temps${RESET}"
        rish_cmd "dumpsys thermalservice 2>/dev/null | grep 'Temperature{'" | head -12 | \
        while read -r line; do
            name=$(echo "$line" | grep -o 'mName=[^,}]*' | cut -d= -f2)
            val=$(echo "$line"  | grep -o 'mValue=[^,}]*' | cut -d= -f2)
            [ -z "$name" ] && continue
            col=$(heat_color "$val")
            bar=$(heat_bar "${val%.*}" 60)
            printf "  ${WHITE}%-16s${RESET} ${col}%5s°C${RESET}  %b\n" "$name" "$val" "$bar"
        done
        echo ""
        return
    fi

    echo "$out" | while read -r line; do
        name=$(echo "$line" | grep -o 'mName=[^,}]*' | cut -d= -f2)
        val=$(echo "$line"  | grep -o 'mValue=[^,}]*' | cut -d= -f2)
        [ -z "$name" ] && continue
        col=$(heat_color "$val")
        bar=$(heat_bar "${val%.*}" 60)
        printf "  ${WHITE}%-16s${RESET} ${col}%5s°C${RESET}  %b\n" "$name" "$val" "$bar"
    done
    echo ""
}

# ── Action menu ───────────────────────────────────────────────────────────────
menu() {
    echo -e "${R2}── Actions ───────────────────────────────────────────────────────${RESET}"
    echo -e "  ${R3}[1]${RESET} Last leatherheadd scan results"
    echo -e "  ${R2}[2]${RESET} Live CPU frequencies"
    echo -e "  ${R1}[3]${RESET} Live HAL temperatures"
    echo -e "  ${R0}[4]${RESET} Full live view (refreshes every 5s)"
    echo -e "  ${R3}[5]${RESET} Force leatherheadd rescan"
    echo -e "  ${DIM}[q]${RESET} Quit"
    echo ""
    read -p "$(echo -e "${R2}Choice: ${RESET}")" choice

    case "$choice" in
        1) header; show_last_scan ;;
        2) header; live_cpu ;;
        3) header; live_hal ;;
        4)
            while true; do
                header
                live_hal
                live_cpu
                echo -e "${DIM}  Refreshing in 5s — Ctrl+C to stop${RESET}"
                sleep 5
            done
            ;;
        5)
            echo -e "${R2}Triggering leatherheadd rescan...${RESET}"
            "$BASE/bin/syndicatectl" scan-count leatherheadd 1 2>/dev/null
            pkill -x leatherheadd 2>/dev/null
            sleep 1
            "$BASE/bin/leatherheadd" >> "$BASE/logs/leatherheadd.log" 2>&1 &
            echo -e "${R3}Daemon restarted — results in ~30s${RESET}"
            sleep 2
            ;;
        q|Q) exit 0 ;;
        *) echo -e "${R0}Invalid choice${RESET}"; sleep 1 ;;
    esac

    menu
}

# ── Main ──────────────────────────────────────────────────────────────────────
header
show_last_scan
menu
