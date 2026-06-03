#!/data/data/com.termux/files/usr/bin/bash
# ==============================================================================
#  Kernel Sanders — Integrity Truth Companion
#  Companion to: shredderd daemon
#  Reads: Registry/daemon_results/shredderd.json
#  Theme: 1980s harvest gold, burnt orange, avocado, cream — retro wallpaper
# ==============================================================================

BASE="$HOME/MiuiserPeruser"
RESULTS="$BASE/Registry/daemon_results/shredderd.json"
ADB="adb -s 127.0.0.1:5555"

# ── Retro palette — harvest gold, burnt orange, avocado, cream ───────────────
HG='\033[38;5;136m'   # harvest gold
BO='\033[38;5;130m'   # burnt orange
AV='\033[38;5;58m'    # avocado green
CR='\033[38;5;223m'   # cream
BG='\033[38;5;94m'    # brown
WA='\033[38;5;180m'   # warm amber
RD='\033[38;5;160m'   # alarm red (contrast)
DK='\033[38;5;238m'   # dark background
WHITE='\033[38;5;255m'
BOLD='\033[1m'
RESET='\033[0m'

COLS=$(tput cols 2>/dev/null || echo 60)
W=$(( COLS - 4 )); [ "$W" -gt 64 ] && W=64

run_cmd() { $ADB shell "$*" 2>/dev/null; }

# ── Retro divider ─────────────────────────────────────────────────────────────
divider() {
    local col=${1:-$HG} char=${2:-~}
    echo -e "${col}$(printf '%*s' "$COLS" | tr ' ' "$char")${RESET}"
}

wallpaper_line() {
    # Repeating retro pattern
    local pat="${HG}▓${BO}░${AV}▒${CR}░${BG}▓${WA}░${RESET}"
    local line=""
    local i=0
    while [ $i -lt $(( COLS / 6 + 1 )) ]; do
        line="${line}${pat}"
        i=$((i+1))
    done
    printf "%.${COLS}s\n" "$(echo -e "$line")"
}

# ── Header ────────────────────────────────────────────────────────────────────
header() {
    clear
    wallpaper_line
    echo ""
    local title="~ K E R N E L   S A N D E R S ~"
    local sub="Kernel integrity companion  /  shredderd daemon"
    local tp=$(( (COLS - ${#title}) / 2 )); [ $tp -lt 0 ] && tp=0
    local sp=$(( (COLS - ${#sub}) / 2 ));   [ $sp -lt 0 ] && sp=0
    printf "%${tp}s${BOLD}${HG}%s${RESET}\n" "" "$title"
    printf "%${sp}s${DK}%s${RESET}\n" "" "$sub"
    echo ""
    wallpaper_line
    echo ""
}

# ── Box helpers ───────────────────────────────────────────────────────────────
box_top() {
    local title=$1 color=$2
    local inner=$(( W - 2 ))
    local pad=$(( (inner - ${#title}) / 2 )); [ $pad -lt 0 ] && pad=0
    echo -e "${color}+$(printf '%*s' "$inner" | tr ' ' '-')+${RESET}"
    printf "${color}|${RESET}%${pad}s${BOLD}${CR}%s${RESET}%$(( inner - pad - ${#title} ))s${color}|${RESET}\n" "" "$title" ""
    echo -e "${color}+$(printf '%*s' "$inner" | tr ' ' '-')+${RESET}"
}
box_bot() { echo -e "${1}+$(printf '%*s' "$(( W - 2 ))" | tr ' ' '-')+${RESET}"; }
box_row() {
    local color=$1 content=$2 plain=$3
    [ -z "$plain" ] && plain="$content"
    local inner=$(( W - 2 ))
    local padlen=$(( inner - ${#plain} - 1 ))
    [ $padlen -lt 0 ] && padlen=0
    printf "${color}|${RESET} %b%${padlen}s${color}|${RESET}\n" "$content" ""
}

# ── Last scan results ─────────────────────────────────────────────────────────
show_last_scan() {
    box_top "LAST INTEGRITY SCAN" "$HG"
    if [ ! -f "$RESULTS" ]; then
        box_row "$HG" "  No results yet — shredderd hasn't scanned" "  No results yet"
        box_bot "$HG"; return
    fi

    local score grade ts signals mods changed
    score=$(jq -r '.integrity_score // "?"' "$RESULTS" 2>/dev/null)
    grade=$(jq -r '.grade // "?"' "$RESULTS" 2>/dev/null)
    ts=$(jq -r '.timestamp // "?"' "$RESULTS" 2>/dev/null)
    signals=$(jq -r '.signals_fired // 0' "$RESULTS" 2>/dev/null)
    mods=$(jq -r '.unknown_modules // 0' "$RESULTS" 2>/dev/null)
    changed=$(jq -r '.changed_binaries // 0' "$RESULTS" 2>/dev/null)

    local sc="$WA"
    case "$grade" in
        CLEAN)     sc="$AV" ;;
        MODIFIED)  sc="$HG" ;;
        SUSPECT)   sc="$BO" ;;
        COMPROMISED) sc="$RD" ;;
    esac

    box_row "$HG" "  Score   : ${sc}${BOLD}${score}/100 [${grade}]${RESET}" "  Score   : ${score}/100 [${grade}]"
    box_row "$HG" "  Scanned : ${DK}${ts}${RESET}" "  Scanned : ${ts}"
    box_row "$HG" "  Signals : ${BO}${signals}${RESET}" "  Signals : ${signals}"
    box_row "$HG" "  Unknown modules  : ${BO}${mods}${RESET}" "  Unknown modules  : ${mods}"
    box_row "$HG" "  Changed binaries : ${BO}${changed}${RESET}" "  Changed binaries : ${changed}"
    box_bot "$HG"
}

# ── Live kernel modules ───────────────────────────────────────────────────────
show_modules() {
    box_top "KERNEL MODULES" "$BO"
    local out
    out=$(run_cmd "cat /proc/modules 2>/dev/null | awk '{print \$1, \$3}' | head -15")
    if [ -z "$out" ]; then
        out=$(run_cmd "lsmod 2>/dev/null | tail -n +2 | awk '{print \$1, \$3}' | head -15")
    fi
    if [ -z "$out" ]; then
        box_row "$BO" "  /proc/modules unavailable" "  /proc/modules unavailable"
        box_bot "$BO"; return
    fi
    echo "$out" | while IFS= read -r line; do
        local mod holders
        mod=$(echo "$line" | awk '{print $1}')
        holders=$(echo "$line" | awk '{print $2}')
        local col="$CR"
        [ "${holders:-0}" -eq 0 ] 2>/dev/null && col="$WA"
        box_row "$BO" "  ${col}${mod}${RESET} ${DK}holders=${holders}${RESET}" "  ${mod} holders=${holders}"
    done
    box_bot "$BO"
}

# ── SELinux status ────────────────────────────────────────────────────────────
show_selinux() {
    box_top "SELINUX + KERNEL PARAMS" "$AV"
    local sel
    sel=$(run_cmd "getenforce 2>/dev/null || cat /sys/fs/selinux/enforce 2>/dev/null")
    sel=$(echo "$sel" | tr -d '\r\n')

    local col="$AV"
    [ "$sel" = "Permissive" ] && col="$BO"
    [ "$sel" = "Disabled" ]   && col="$RD"
    box_row "$AV" "  SELinux : ${col}${BOLD}${sel:-unknown}${RESET}" "  SELinux : ${sel:-unknown}"

    # Key kernel params
    for param in kptr_restrict dmesg_restrict perf_event_paranoid modules_disabled; do
        local val
        val=$(run_cmd "cat /proc/sys/kernel/${param} 2>/dev/null" | tr -d '\r\n')
        local pc="$CR"
        [ "${val:-0}" -eq 0 ] 2>/dev/null && pc="$BO"
        box_row "$AV" "  ${param}: ${pc}${val:-n/a}${RESET}" "  ${param}: ${val:-n/a}"
    done
    box_bot "$AV"
}

# ── System binary quick check ─────────────────────────────────────────────────
show_binaries() {
    box_top "CRITICAL BINARY CHECK" "$BG"
    local critical="/system/bin/su /system/xbin/su /system/bin/busybox /data/local/tmp/su"
    local found=0
    for path in $critical; do
        local exists
        exists=$(run_cmd "test -f ${path} && echo YES || echo NO" | tr -d '\r\n')
        if [ "$exists" = "YES" ]; then
            box_row "$BG" "  ${RD}FOUND${RESET} ${CR}${path}${RESET}" "  FOUND ${path}"
            found=$((found+1))
        fi
    done
    [ $found -eq 0 ] && box_row "$BG" "  ${AV}OK${RESET} No suspicious binaries found" "  OK No suspicious binaries found"

    # Check for magisk
    local magisk
    magisk=$(run_cmd "pm list packages 2>/dev/null | grep -i magisk" | tr -d '\r')
    if [ -n "$magisk" ]; then
        box_row "$BG" "  ${RD}MAGISK${RESET} ${CR}${magisk}${RESET}" "  MAGISK ${magisk}"
    fi
    box_bot "$BG"
}

# ── Force rescan ──────────────────────────────────────────────────────────────
rescan() {
    echo -e "${HG}Triggering shredderd rescan...${RESET}"
    pkill -x shredderd 2>/dev/null
    sleep 1
    "$BASE/bin/shredderd" >> "$BASE/logs/shredderd.log" 2>&1 &
    echo -e "${AV}Done — results in ~15s${RESET}"
    sleep 2
}

# ── Watch mode ────────────────────────────────────────────────────────────────
watch_mode() {
    while true; do
        header
        show_last_scan; echo ""
        show_selinux;   echo ""
        show_binaries;  echo ""
        divider "$HG" "~"
        echo -e "${DK}  Refreshing in 10s — Ctrl+C to stop${RESET}"
        sleep 10
    done
}

# ── Menu ──────────────────────────────────────────────────────────────────────
menu() {
    divider "$HG" "~"
    echo ""
    echo -e "  ${HG}[1]${RESET} Last shredderd scan"
    echo -e "  ${BO}[2]${RESET} Live kernel modules"
    echo -e "  ${AV}[3]${RESET} SELinux + kernel params"
    echo -e "  ${BG}[4]${RESET} Critical binary check"
    echo -e "  ${WA}[5]${RESET} Watch mode (10s refresh)"
    echo -e "  ${BO}[6]${RESET} Force shredderd rescan"
    echo -e "  ${DK}[q]${RESET} Quit"
    echo ""
    read -r -p "$(echo -e "${HG}Choice: ${RESET}")" choice

    case "$choice" in
        1) header; show_last_scan; echo ""; menu ;;
        2) header; show_modules; echo ""; menu ;;
        3) header; show_selinux; echo ""; menu ;;
        4) header; show_binaries; echo ""; menu ;;
        5) watch_mode ;;
        6) rescan; menu ;;
        q|Q) exit 0 ;;
        *) echo -e "${BO}Invalid${RESET}"; sleep 1; menu ;;
    esac
}

# ── Main ──────────────────────────────────────────────────────────────────────
header
show_last_scan
echo ""
show_selinux
echo ""
show_binaries
echo ""
menu
