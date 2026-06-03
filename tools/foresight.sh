#!/data/data/com.termux/files/usr/bin/bash
# ==============================================================================
#  Foresight — Foreground & Crash Intelligence Tool
#  Companion to: fugitoidd daemon
#  Reads: Registry/daemon_results/fugitoidd.json
#  Theme: Brat green — acidic lime, electric, chaotic energy
# ==============================================================================

BASE="$HOME/MiuiserPeruser"
RESULTS="$BASE/Registry/daemon_results/fugitoidd.json"
ADB="adb -s 127.0.0.1:5555"

# ── Brat palette ──────────────────────────────────────────────────────────────
BRAT='\033[38;5;154m'      # brat green — acidic lime
BRAT2='\033[38;5;118m'     # slightly darker lime
BRAT3='\033[38;5;82m'      # deep brat
BRATBG='\033[48;5;154m'    # brat background block
BLACK='\033[38;5;16m'      # pure black on brat bg
CHALK='\033[38;5;253m'     # off white
GREY='\033[38;5;242m'      # mid grey
DARK='\033[38;5;235m'      # near black
RED='\033[38;5;196m'       # crash red
ORANGE='\033[38;5;208m'    # ANR orange
BOLD='\033[1m'
RESET='\033[0m'

COLS=$(tput cols 2>/dev/null || echo 64)
run_cmd() { $ADB shell "$*" 2>/dev/null; }

# ── Brat dividers ─────────────────────────────────────────────────────────────
brat_rule() {
    echo -e "${BRAT}$(printf '%*s' "$COLS" | tr ' ' '*')${RESET}"
}

thin_rule() {
    echo -e "${BRAT3}$(printf '%*s' "$COLS" | tr ' ' '-')${RESET}"
}

brat_block() {
    local text=$1
    local inner=$(( COLS - 2 ))
    local pad=$(( (inner - ${#text}) / 2 )); [ $pad -lt 0 ] && pad=0
    local rpad=$(( inner - pad - ${#text} )); [ $rpad -lt 0 ] && rpad=0
    echo -e "${BRATBG}${BLACK}${BOLD}$(printf '%*s' "$COLS" | tr ' ' ' ')${RESET}"
    printf "${BRATBG}${BLACK}${BOLD} %${pad}s%s%${rpad}s ${RESET}\n" "" "$text" ""
    echo -e "${BRATBG}${BLACK}${BOLD}$(printf '%*s' "$COLS" | tr ' ' ' ')${RESET}"
}

# ── Header ────────────────────────────────────────────────────────────────────
header() {
    clear
    brat_rule
    brat_block "F O R E S I G H T"
    local sub="Foreground + Crash Intelligence  /  fugitoidd daemon"
    local sp=$(( (COLS - ${#sub}) / 2 )); [ $sp -lt 0 ] && sp=0
    printf "%${sp}s${GREY}%s${RESET}\n" "" "$sub"
    brat_rule
    echo ""
}

# ── Last scan ─────────────────────────────────────────────────────────────────
show_last_scan() {
    thin_rule
    echo -e "  ${BOLD}${BRAT}LAST FUGITOIDD SCAN${RESET}"
    thin_rule

    if [ ! -f "$RESULTS" ]; then
        echo -e "  ${GREY}No results yet — fugitoidd hasn't scanned${RESET}"
        echo ""; return
    fi

    local ts fg crashes anrs ooms signals
    ts=$(jq -r '.timestamp // "?"' "$RESULTS" 2>/dev/null)
    fg=$(jq -r '.foreground_app // "unknown"' "$RESULTS" 2>/dev/null)
    crashes=$(jq -r '.crash_count // 0' "$RESULTS" 2>/dev/null)
    anrs=$(jq -r '.anr_count // 0' "$RESULTS" 2>/dev/null)
    ooms=$(jq -r '.oom_count // 0' "$RESULTS" 2>/dev/null)
    signals=$(jq -r '.signals_fired // 0' "$RESULTS" 2>/dev/null)

    printf "  ${GREY}Scanned  :${RESET} ${DARK}%s${RESET}\n" "$ts"
    printf "  ${GREY}Foreground:${RESET} ${BRAT}${BOLD}%s${RESET}\n" "$fg"

    local cc="$BRAT2"; [ "${crashes:-0}" -gt 0 ] && cc="$RED"
    local ac="$BRAT2"; [ "${anrs:-0}" -gt 0 ]    && ac="$ORANGE"
    local oc="$BRAT2"; [ "${ooms:-0}" -gt 0 ]     && oc="$RED"

    printf "  ${GREY}Crashes  :${RESET} ${cc}${BOLD}%s${RESET}\n" "$crashes"
    printf "  ${GREY}ANRs     :${RESET} ${ac}${BOLD}%s${RESET}\n" "$anrs"
    printf "  ${GREY}OOM kills:${RESET} ${oc}${BOLD}%s${RESET}\n" "$ooms"
    printf "  ${GREY}Signals  :${RESET} ${BRAT}%s${RESET}\n" "$signals"
    echo ""
}

# ── Live foreground ───────────────────────────────────────────────────────────
show_foreground() {
    thin_rule
    echo -e "  ${BOLD}${BRAT}LIVE FOREGROUND APP${RESET}"
    thin_rule

    local fg uid pid
    fg=$(run_cmd "dumpsys activity activities | grep -E 'mResumedActivity|ResumedActivity' | head -1" | tr -d '\r' | sed 's/.*\///' | sed 's/}.*//')
    uid=$(run_cmd "dumpsys activity activities | grep -E 'mResumedActivity|ResumedActivity' | head -1" | tr -d '\r' | grep -o 'u[0-9]*[a-z]*[0-9]*' | head -1)

    if [ -z "$fg" ]; then
        echo -e "  ${GREY}Unable to detect foreground app${RESET}"
    else
        printf "  ${BRAT}${BOLD}%s${RESET}\n" "$fg"
        printf "  ${GREY}User: %s${RESET}\n" "${uid:-unknown}"
    fi
    echo ""

    # Recent activity stack
    echo -e "  ${GREY}Recent activity stack:${RESET}"
    run_cmd "dumpsys activity activities | grep 'Run #' | head -8" | tr -d '\r' | \
    while IFS= read -r line; do
        printf "  ${DARK}%s${RESET}\n" "$line"
    done
    echo ""
}

# ── Live crash log ────────────────────────────────────────────────────────────
show_crashes() {
    thin_rule
    echo -e "  ${BOLD}${BRAT}RECENT CRASHES + ANRs${RESET}"
    thin_rule

    local crashes anrs ooms
    crashes=$(run_cmd "logcat -d -t 100 2>/dev/null | grep -iE 'FATAL|AndroidRuntime' | tail -5")
    anrs=$(run_cmd "logcat -d -t 100 2>/dev/null | grep -i 'ANR in' | tail -5")
    ooms=$(run_cmd "logcat -d -t 100 2>/dev/null | grep -i 'Low on memory\|OutOfMemory\|OOM' | tail -3")

    if [ -n "$crashes" ]; then
        echo -e "  ${RED}${BOLD}CRASHES:${RESET}"
        echo "$crashes" | tr -d '\r' | while IFS= read -r line; do
            echo -e "  ${RED}${line}${RESET}" | cut -c1-$COLS
        done
        echo ""
    fi

    if [ -n "$anrs" ]; then
        echo -e "  ${ORANGE}${BOLD}ANRs:${RESET}"
        echo "$anrs" | tr -d '\r' | while IFS= read -r line; do
            echo -e "  ${ORANGE}${line}${RESET}" | cut -c1-$COLS
        done
        echo ""
    fi

    if [ -n "$ooms" ]; then
        echo -e "  ${RED}${BOLD}OOM KILLS:${RESET}"
        echo "$ooms" | tr -d '\r' | while IFS= read -r line; do
            echo -e "  ${RED}${line}${RESET}" | cut -c1-$COLS
        done
        echo ""
    fi

    [ -z "$crashes" ] && [ -z "$anrs" ] && [ -z "$ooms" ] && \
        echo -e "  ${BRAT}No crashes or ANRs in recent log${RESET}" && echo ""
}

# ── Top memory consumers ──────────────────────────────────────────────────────
show_memory() {
    thin_rule
    echo -e "  ${BOLD}${BRAT}TOP MEMORY CONSUMERS${RESET}"
    thin_rule

    local out
    out=$(run_cmd "dumpsys meminfo | grep -E '^\s+[0-9]' | sort -rn | head -12")
    if [ -z "$out" ]; then
        out=$(run_cmd "cat /proc/meminfo | head -8")
    fi

    echo "$out" | tr -d '\r' | while IFS= read -r line; do
        echo -e "  ${GREY}${line}${RESET}"
    done
    echo ""
}

# ── Investigate an app ────────────────────────────────────────────────────────
investigate() {
    header
    echo -e "  ${BRAT}${BOLD}INVESTIGATE AN APP${RESET}"
    thin_rule
    echo ""
    echo -e "  ${GREY}Enter package name (e.g. com.google.android.gms):${RESET}"
    read -r -p "$(echo -e "  ${BRAT}>> ${RESET}")" pkg
    [ -z "$pkg" ] && return

    header
    echo -e "  ${BRAT}${BOLD}INVESTIGATING: ${CHALK}${pkg}${RESET}"
    thin_rule
    echo ""

    # Memory usage
    echo -e "  ${GREY}Memory:${RESET}"
    run_cmd "dumpsys meminfo $pkg 2>/dev/null | grep -E 'TOTAL|Heap|PSS' | head -5" | tr -d '\r' | \
    while IFS= read -r line; do echo -e "  ${DARK}${line}${RESET}"; done
    echo ""

    # Recent logcat
    echo -e "  ${GREY}Recent log activity:${RESET}"
    run_cmd "logcat -d -t 30 2>/dev/null | grep -i \"$pkg\" | tail -8" | tr -d '\r' | \
    while IFS= read -r line; do
        local col="$DARK"
        echo "$line" | grep -qiE "crash|fatal|anr|error|exception" && col="$RED"
        echo -e "  ${col}${line}${RESET}" | cut -c1-$COLS
    done
    echo ""

    # Actions
    thin_rule
    echo -e "  ${BRAT}[1]${RESET} Force stop app"
    echo -e "  ${BRAT2}[2]${RESET} Clear app cache"
    echo -e "  ${ORANGE}[3]${RESET} Restrict background activity"
    echo -e "  ${RED}[4]${RESET} Revoke all permissions"
    echo -e "  ${DARK}[q]${RESET} Back"
    echo ""
    read -r -p "$(echo -e "  ${BRAT}>> ${RESET}")" action

    case "$action" in
        1) run_cmd "am force-stop $pkg"
           echo -e "  ${BRAT}Force stopped ${pkg}${RESET}"; sleep 2 ;;
        2) run_cmd "pm clear $pkg"
           echo -e "  ${BRAT}Cache cleared for ${pkg}${RESET}"; sleep 2 ;;
        3) run_cmd "appops set $pkg RUN_IN_BACKGROUND deny"
           echo -e "  ${BRAT}Background restricted for ${pkg}${RESET}"; sleep 2 ;;
        4) run_cmd "pm revoke $pkg android.permission.CAMERA 2>/dev/null"
           run_cmd "pm revoke $pkg android.permission.RECORD_AUDIO 2>/dev/null"
           run_cmd "pm revoke $pkg android.permission.ACCESS_FINE_LOCATION 2>/dev/null"
           echo -e "  ${ORANGE}Key permissions revoked for ${pkg}${RESET}"; sleep 2 ;;
        q|Q) return ;;
    esac
}

# ── Watch mode ────────────────────────────────────────────────────────────────
watch_mode() {
    while true; do
        header
        show_last_scan
        show_foreground
        brat_rule
        echo -e "  ${DARK}Refreshing in 10s — Ctrl+C to stop${RESET}"
        sleep 10
    done
}

# ── Rescan ────────────────────────────────────────────────────────────────────
rescan() {
    echo -e "${BRAT}Triggering fugitoidd rescan...${RESET}"
    pkill -x fugitoidd 2>/dev/null; sleep 1
    "$BASE/bin/fugitoidd" >> "$BASE/logs/fugitoidd.log" 2>&1 &
    echo -e "${BRAT2}Done — results in ~5s${RESET}"; sleep 5
}

# ── Menu ──────────────────────────────────────────────────────────────────────
menu() {
    brat_rule
    echo ""
    echo -e "  ${BRAT}[1]${RESET} Last fugitoidd scan"
    echo -e "  ${BRAT2}[2]${RESET} Live foreground app"
    echo -e "  ${BRAT3}[3]${RESET} Recent crashes + ANRs"
    echo -e "  ${CHALK}[4]${RESET} Top memory consumers"
    echo -e "  ${BRAT}[5]${RESET} Investigate + enforce"
    echo -e "  ${BRAT2}[6]${RESET} Watch mode (10s)"
    echo -e "  ${BRAT3}[7]${RESET} Force rescan"
    echo -e "  ${DARK}[q]${RESET} Quit"
    echo ""
    read -r -p "$(echo -e "  ${BRAT}>> ${RESET}")" choice
    case "$choice" in
        1) header; show_last_scan; menu ;;
        2) header; show_foreground; menu ;;
        3) header; show_crashes; menu ;;
        4) header; show_memory; menu ;;
        5) investigate; menu ;;
        6) watch_mode ;;
        7) rescan; menu ;;
        q|Q) exit 0 ;;
        *) echo -e "  ${BRAT3}Invalid${RESET}"; sleep 1; menu ;;
    esac
}

# ── Main ──────────────────────────────────────────────────────────────────────
header
show_last_scan
menu
