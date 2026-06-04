#!/data/data/com.termux/files/usr/bin/bash
source "$(dirname "$0")/lib/tool_backend.sh"

# -------------------------------------------------------------------
#  Discover — Privacy Auditor (The Double D's)
# -------------------------------------------------------------------
#  Scans apps for dangerous permissions and network activity.
#  Flags threats for Destroyer (Daemon Hunter).
# -------------------------------------------------------------------

export PATH="$PATH:$HOME/.shizuku:$PREFIX/bin"
source lib/miuiserperuser_common.sh 2>/dev/null || true
db_log_tool_start "Discover"

# Colors
BOLD='\033[1m'; DIM='\033[2m'; RESET='\033[0m'
C_BLUE='\033[38;5;39m'; C_GREEN='\033[38;5;48m'; C_YELLOW='\033[38;5;221m'
C_ORANGE='\033[38;5;214m'; C_RED='\033[38;5;203m'; C_PURPLE='\033[38;5;141m'
C_GRAY='\033[38;5;245m'; C_WHITE='\033[38;5;255m'
C_CYAN='\033[0;36m'

BOX_H='─'; BOX_V='│'
BOX_TL_THICK='╔'; BOX_TR_THICK='╗'; BOX_BL_THICK='╚'; BOX_BR_THICK='╝'

TMP_DIR="data/tmp"
mkdir -p "$TMP_DIR"

# ------------------------------------------------------------
#  Shell Dispatcher
# ------------------------------------------------------------
run_shell() {
    if [[ -x "$HOME/rish" ]]; then
        "$HOME/rish" -c "$*" 2>/dev/null
    elif command -v adb >/dev/null 2>&1; then
        adb shell "$@" 2>/dev/null
    else
        return 1
    fi
}

# ------------------------------------------------------------
#  Connection Test
# ------------------------------------------------------------
clear
echo -e "${C_BLUE}${BOX_TL_THICK}$(printf '%*s' 76 | tr ' ' "${BOX_H}")${BOX_TR_THICK}${RESET}"
echo -e "${C_BLUE}${BOX_V}${RESET} ${BOLD}${C_WHITE}🔍 DISCOVER — Privacy Auditor${RESET}"
echo -e "${C_BLUE}${BOX_V}${RESET} ${DIM}${C_GRAY}The Double D's — Discover threats, Destroyer enforces${RESET}"
echo -e "${C_BLUE}${BOX_BL_THICK}$(printf '%*s' 76 | tr ' ' "${BOX_H}")${BOX_BR_THICK}${RESET}"
echo ""

echo -e "${C_CYAN}📡 Connecting to device...${RESET}"

if [[ -x "$HOME/rish" ]] && "$HOME/rish" -c "getprop ro.product.model" 2>/dev/null | grep -q .; then
    echo -e "${C_GREEN}   ✅ Connected via Shizuku rish${RESET}"
elif adb shell getprop ro.product.model 2>/dev/null | grep -q .; then
    echo -e "${C_GREEN}   ✅ Connected via ADB${RESET}"
else
    echo -e "${C_RED}   ❌ No connection.${RESET}"
    exit 1
fi

# ------------------------------------------------------------
#  Scan Mode Selection
# ------------------------------------------------------------
echo ""
echo -e "${C_PURPLE}┌─── SCAN OPTIONS ────────────────────────────────────────────────────────┐${RESET}"
echo -e "${C_PURPLE}${BOX_V}${RESET}   [1] Scan all installed apps                                      ${C_PURPLE}${BOX_V}${RESET}"
echo -e "${C_PURPLE}${BOX_V}${RESET}   [2] Scan only Xiaomi/MIUI apps (com.miui.* / com.xiaomi.*)        ${C_PURPLE}${BOX_V}${RESET}"
echo -e "${C_PURPLE}${BOX_V}${RESET}   [3] Scan running apps only (faster)                                 ${C_PURPLE}${BOX_V}${RESET}"
echo -e "${C_PURPLE}└───────────────────────────────────────────────────────────────────────────┘${RESET}"
echo ""
read -p "Choose scan mode [1-3]: " scan_mode

case $scan_mode in
    1) package_filter="." ;;
    2) package_filter="com.miui|com.xiaomi" ;;
    3) package_filter="."; running_only=1 ;;
    *) echo -e "${C_RED}Invalid choice.${RESET}"; exit 1 ;;
esac

# ------------------------------------------------------------
#  Scan Packages
# ------------------------------------------------------------
echo ""
echo -e "${C_GRAY}🔍 Gathering package list...${RESET}"

if [[ $running_only -eq 1 ]]; then
    run_shell "ps -A -o NAME" 2>/dev/null | grep -v "NAME" | awk '{print $NF}' | sort -u > "$TMP_DIR/packages.txt"
else
    run_shell "pm list packages" 2>/dev/null | sed 's/package://' > "$TMP_DIR/packages.txt"
fi

# Filter
grep -E "$package_filter" "$TMP_DIR/packages.txt" > "$TMP_DIR/filtered.txt"
total_packages=$(wc -l < "$TMP_DIR/filtered.txt")
echo -e "${C_GREEN}   ✅ Found $total_packages packages to analyze${RESET}"

# ------------------------------------------------------------
#  Analyze Each Package
# ------------------------------------------------------------
declare -a results
index=0

# Dangerous permissions list
DANGEROUS_PERMS="CAMERA|RECORD_AUDIO|ACCESS_FINE_LOCATION|READ_CONTACTS|READ_SMS|READ_CALL_LOG|WRITE_EXTERNAL_STORAGE"

while read -r pkg; do
    [[ -z "$pkg" ]] && continue
    index=$((index + 1))
    
    printf "\r${C_GRAY}   Analyzing %d/%d: %s...${RESET}" "$index" "$total_packages" "${pkg:0:40}"
    
    # Get dangerous permissions count
    perms=$(run_shell "dumpsys package $pkg" 2>/dev/null | grep -E "granted=true|permission" | grep -oE "android.permission.[A-Z_]+")
    dangerous_count=$(echo "$perms" | grep -cE "$DANGEROUS_PERMS")
    [[ -z "$dangerous_count" ]] && dangerous_count=0
    
    # Get network activity (KB)
    uid=$(run_shell "pm list packages -U" 2>/dev/null | grep "$pkg" | head -1 | sed -n 's/.*uid://p')
    if [[ -n "$uid" ]]; then
        rx_bytes=$(run_shell "dumpsys netstats detail" 2>/dev/null | grep -A10 "uid=$uid" | grep -oE "rb=[0-9]+" | tail -1 | grep -oE '[0-9]+')
        tx_bytes=$(run_shell "dumpsys netstats detail" 2>/dev/null | grep -A10 "uid=$uid" | grep -oE "tb=[0-9]+" | tail -1 | grep -oE '[0-9]+')
    fi
    [[ -z "$rx_bytes" ]] && rx_bytes=0
    [[ -z "$tx_bytes" ]] && tx_bytes=0
    total_kb=$(((rx_bytes + tx_bytes) / 1024))
    
    # Check if currently running
    run_shell "pgrep -f $pkg" > /dev/null 2>&1 && is_running=1 || is_running=0
    
    # Calculate Privacy Score
    score=100
    score=$((score - (dangerous_count * 15)))
    score=$((score - (total_kb / 1024)))  # -1 per MB
    [[ $is_running -eq 1 ]] && score=$((score - 10))
    [[ $score -lt 0 ]] && score=0
    
    # Determine rating
    if [[ $score -ge 80 ]]; then
        rating="✅ Trusted"
        rating_color="$C_GREEN"
        action="ignore"
    elif [[ $score -ge 50 ]]; then
        rating="⚠️ Caution"
        rating_color="$C_YELLOW"
        action="restrict"
    elif [[ $score -ge 20 ]]; then
        rating="🔴 Suspicious"
        rating_color="$C_ORANGE"
        action="starve"
    else
        rating="💀 Critical"
        rating_color="$C_RED"
        action="deep_starve"
    fi
    
    # Store result
    results+=("$pkg|$score|$dangerous_count|$total_kb|$is_running|$action|$rating|$rating_color")
done < "$TMP_DIR/filtered.txt"

echo ""
echo ""

# ------------------------------------------------------------
#  Display Results (Worst Offenders First)
# ------------------------------------------------------------
clear
echo -e "${C_BLUE}${BOX_TL_THICK}$(printf '%*s' 76 | tr ' ' "${BOX_H}")${BOX_TR_THICK}${RESET}"
echo -e "${C_BLUE}${BOX_V}${RESET} ${BOLD}${C_WHITE}🔍 DISCOVER — Privacy Audit Results${RESET}"
echo -e "${C_BLUE}${BOX_V}${RESET} ${DIM}${C_GRAY}Worst offenders (lowest privacy score = most invasive)${RESET}"
echo -e "${C_BLUE}${BOX_BL_THICK}$(printf '%*s' 76 | tr ' ' "${BOX_H}")${BOX_BR_THICK}${RESET}"
echo ""

# Sort by score (lowest first) and show top 15
count=0
printf "%s\n" "${results[@]}" | sort -t'|' -k2 -n | head -15 | while IFS='|' read -r pkg score dangerous kb running action rating rating_color; do
    count=$((count + 1))
    
    echo -e "${C_PURPLE}┌─── #${count} ─────────────────────────────────────────────────────────────────┐${RESET}"
    printf "${C_PURPLE}${BOX_V}${RESET} ${C_WHITE}%s${RESET}\n" "$pkg"
    printf "${C_PURPLE}${BOX_V}${RESET}   Score: ${rating_color}%d${RESET} — %b\n" "$score" "$rating_color$rating${RESET}"
    printf "${C_PURPLE}${BOX_V}${RESET}   ${C_GRAY}Dangerous Perms: %d  |  Network: %d KB  |  Running: %s${RESET}\n" "$dangerous" "$kb" "$([ $running -eq 1 ] && echo "Yes" || echo "No")"
    printf "${C_PURPLE}${BOX_V}${RESET}   → Recommended: ${C_WHITE}%s${RESET}\n" "$action"
    echo -e "${C_PURPLE}└───────────────────────────────────────────────────────────────────────────┘${RESET}"
    echo ""
done

# ------------------------------------------------------------
#  Action Menu
# ------------------------------------------------------------
echo -e "${C_CYAN}──────────────────────────────────────────────────────────────────────────────${NC}"
echo "What would you like to do?"
echo "  [1] Add ALL suspicious/critical packages to Destroyer brain"
echo "  [2] Add SELECTED packages to Destroyer brain"
echo "  [3] Exit (do nothing)"
echo ""
read -p "Choice [1-3]: " choice

BRAIN="data/daemonhunter_brain.txt"
touch "$BRAIN"

case $choice in
    1)
        added=0
        printf "%s\n" "${results[@]}" | sort -t'|' -k2 -n | while IFS='|' read -r pkg score dangerous kb running action rating rating_color; do
            if [[ "$action" == "starve" || "$action" == "deep_starve" ]]; then
                if ! grep -q "^$pkg|" "$BRAIN" 2>/dev/null; then
            ./bin/daemonhunterd --add "$pkg" "$action"
                    echo -e "${C_GREEN}   ✅ Added $pkg → $action${RESET}"
                    db_log_action "Discover" "$pkg" "$action" "auto"
                    added=$((added + 1))
                fi
            fi
        done
        echo ""
        echo -e "${C_GREEN}✅ Added packages to Destroyer brain.${RESET}"
        echo -e "${C_CYAN}Run './bin/daemonhunterd' to start enforcement.${RESET}"
        ;;
    2)
        echo -e "${C_GRAY}Enter package name to add (copy from above):${RESET}"
        read -p "Package: " pkg
        echo -n "Action [s=starve / d=deep_starve]: "
        read -r act
        if [[ "$act" == "d" ]]; then action="deep_starve"; else action="starve"; fi
        if [[ -n "$pkg" ]]; then
            ./bin/daemonhunterd --add "$pkg" "$action"
            echo -e "${C_GREEN}   ✅ Added $pkg → $action${RESET}"
            db_log_action "Discover" "$pkg" "$action" "manual"
        fi
        ;;
    *) echo -e "${C_GRAY}No changes made.${RESET}" ;;
esac

rm -f "$TMP_DIR"/*.txt
db_log_tool_end "Discover" "0"

echo ""
read -n 1 -s -p "Press any key to return..."
