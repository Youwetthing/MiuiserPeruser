#!/data/data/com.termux/files/usr/bin/bash

# -------------------------------------------------------------------
#  Peruse Control — MIUI System App Oracle (Interactive Teacher)
# -------------------------------------------------------------------
#  Learn before you burn. Completion required to unlock RAM Slammer.
# -------------------------------------------------------------------

# Fallback logging (if library missing)
db_log_tool_start() { :; }
db_log_tool_end() { :; }
db_log_action() { :; }

source lib/miuiserperuser_common.sh 2>/dev/null || true
db_log_tool_start "PeruseControl"

BRAIN="data/daemonhunter_brain.txt"
COMPLETION_MARKER="data/.peruse_completed"

mkdir -p data logs
touch "$BRAIN"

GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; BLUE='\033[0;34m'; CYAN='\033[0;36m'; NC='\033[0m'

# ------------------------------------------------------------
#  Shell dispatcher
# ------------------------------------------------------------
run_shell() {
    if [[ -x "$HOME/.shizuku/rish" ]]; then
        "$HOME/.shizuku/rish" -c "$*" 2>/dev/null
    elif command -v adb >/dev/null 2>&1; then
        adb shell "$@" 2>/dev/null
    else
        return 1
    fi
}

is_system_app() { run_shell "pm list packages -s" 2>/dev/null | grep -q "$1"; }

revoke_dangerous_perms() {
    for op in 27 26 4 6 14 1 60; do
        run_shell "cmd appops set $1 $op ignore" 2>/dev/null
    done
}

deep_starve_pkg() {
    local pkg="$1"; local base_pkg="${pkg%%:*}"
    run_shell "pm suspend $base_pkg" 2>/dev/null
    run_shell "pm disable-user --user 0 $base_pkg" 2>/dev/null
    run_shell "am force-stop $pkg" 2>/dev/null
    run_shell "pm clear $base_pkg" 2>/dev/null
    for op in 63 64 40 59 27 26 4 6 14 1 60; do
        run_shell "cmd appops set $pkg $op ignore" 2>/dev/null
    done
    run_shell "am force-stop $pkg" 2>/dev/null
}

confirm_action() {
    local pkg="$1"; local action="$2"; local risk="$3"
    echo -e "${RED}⚠️  WARNING: You are about to ${action} ${pkg}${NC}"
    echo -e "${RED}   Risk level: ${risk}${NC}"
    echo -n "Type 'yes' to confirm, anything else to cancel: "
    read -r confirm
    [[ "$confirm" == "yes" ]] && return 0 || return 1
}

# ------------------------------------------------------------
#  Knowledge Base
# ------------------------------------------------------------
declare -A PKG_INFO
PKG_INFO["com.miui.home"]="SYSTEM LAUNCHER: DO NOT KILL OR DISABLE.|Critical"
PKG_INFO["com.miui.securitycore"]="SECURITY CORE: Permission controller. DO NOT DISABLE.|Critical"
PKG_INFO["com.miui.notification"]="NOTIFICATION MANAGER: DO NOT KILL.|Critical"
PKG_INFO["com.miui.analytics"]="TELEMETRY: Tracks usage. Safe to DEEP_STARVE.|Safe"
PKG_INFO["com.miui.msa.global"]="ADVERTISING ID: System‑wide ads. Safe to DEEP_STARVE.|Safe"
PKG_INFO["com.miui.daemon"]="BACKGROUND TRACKER: May affect notifications if disabled.|Caution"
PKG_INFO["com.miui.cleanmaster"]="JUNK CLEANER: Aggressive optimizer. Safe to STARVE.|Safe"
PKG_INFO["com.miui.powerkeeper"]="BATTERY MANAGER: Disabling may increase idle drain.|Caution"
PKG_INFO["com.miui.cloudservice"]="XIAOMI CLOUD: Safe to STARVE if unused.|Safe"
PKG_INFO["com.miui.guardprovider"]="ANTIVIRUS AGGREGATOR: DEEP_STARVE.|Safe"
PKG_INFO["com.miui.weather2"]="WEATHER APP: STARVE if unused.|Safe"
PKG_INFO["com.miui.miwallpaper"]="LIVE WALLPAPER: DEEP_STARVE.|Safe"
PKG_INFO["com.xiaomi.discover"]="DISCOVER/ADS: DEEP_STARVE.|Safe"
PKG_INFO["com.xiaomi.xmsf"]="XIAOMI SERVICE FRAMEWORK: Breaks Mi Cloud sync if disabled.|Caution"
PKG_INFO["com.xiaomi.aicr"]="HYPERAI ENGINE: DEEP_STARVE if unused.|Safe"
PKG_INFO["com.miui.notes"]="NOTES APP: Requests mic/contacts. RESTRICT or STARVE.|Safe"
PKG_INFO["com.miui.screenrecorder"]="SCREEN RECORDER: Requests mic. RESTRICT or STARVE.|Safe"
PKG_INFO["com.miui.aod"]="ALWAYS‑ON DISPLAY: RESTRICT or STARVE.|Safe"
PKG_INFO["com.miui.backup"]="LOCAL BACKUP: RESTRICT if unused.|Safe"
PKG_INFO["com.miui.cotaservice"]="CARRIER OTA: RESTRICT permissions.|Safe"
PKG_INFO["com.miui.misightservice"]="DIAGNOSTICS: DEEP_STARVE safe.|Safe"
PKG_INFO["com.miui.yellowpage"]="YELLOW PAGES: STARVE safe.|Safe"
PKG_INFO["com.miui.bugreport"]="BUG REPORTING: STARVE safe.|Safe"
PKG_INFO["com.miui.calculator"]="CALCULATOR: Safe to uninstall.|Safe"
PKG_INFO["com.miui.vsimcore"]="VIRTUAL SIM: STARVE if unused.|Safe"
PKG_INFO["com.xiaomi.account"]="XIAOMI ACCOUNT: Breaks themes/sync if disabled.|Caution"

# ------------------------------------------------------------
#  Interactive Teacher Mode
# ------------------------------------------------------------
interactive_teacher() {
    clear
    echo -e "${CYAN}==============================================================================${NC}"
    echo -e "${CYAN}   📖 INTERACTIVE TEACHER — Learn Before You Burn${NC}"
    echo -e "${CYAN}==============================================================================${NC}"
    echo ""
    echo -e "You will be shown each MIUI package with a description and risk level."
    echo -e "Choose an action for each. Dangerous actions require confirmation."
    echo -e "Completion of this training unlocks RAM Slammer."
    echo ""
    read -n 1 -s -p "Press any key to begin..."

    # Build package list from knowledge base + brain
    declare -A seen
    for pkg in "${!PKG_INFO[@]}"; do seen["$pkg"]=1; done
    while IFS='|' read -r pkg _; do [[ -n "$pkg" ]] && seen["$pkg"]=1; done < "$BRAIN"
    pkg_list=("${!seen[@]}")
    total=${#pkg_list[@]}
    index=0
    visited_last=0

    while true; do
        clear
        pkg="${pkg_list[$index]}"
        info_entry="${PKG_INFO[$pkg]}"
        if [[ -z "$info_entry" ]]; then
            info="No description available. Treat with CAUTION."
            risk="Caution"
        else
            info="${info_entry%%|*}"
            risk="${info_entry##*|}"
        fi

        case "$risk" in
            Critical) color="$RED"; risk_text="⛔ CRITICAL"; default_advice="DO NOT TOUCH. IGNORE." ;;
            Caution) color="$YELLOW"; risk_text="⚠️ CAUTION"; default_advice="Consider restricting or starve cautiously." ;;
            Safe) color="$GREEN"; risk_text="✅ SAFE"; default_advice="Safe to deep starve or disable." ;;
            *) color="$CYAN"; risk_text="ℹ️ INFO"; default_advice="Review manually." ;;
        esac

        echo -e "${CYAN}==============================================================================${NC}"
        echo -e "${CYAN}   Package $((index+1)) of $total${NC}"
        echo -e "${CYAN}==============================================================================${NC}"
        echo ""
        echo -e "${color}┌────────────────────────────────────────────────────────────────────────────┐${NC}"
        printf "${color}│${NC} ${BLUE}%-74s${NC} ${color}│${NC}\n" "$pkg"
        echo -e "${color}│${NC} ${color}[${risk_text}]${NC}                                                                        ${color}│${NC}"
        echo -e "${color}├────────────────────────────────────────────────────────────────────────────┤${NC}"
        printf "${color}│${NC} %-74s ${color}│${NC}\n" "📋 $info"
        echo -e "${color}│${NC}                                                                            ${color}│${NC}"
        printf "${color}│${NC} ${YELLOW}→ %-72s${NC} ${color}│${NC}\n" "$default_advice"
        echo -e "${color}└────────────────────────────────────────────────────────────────────────────┘${NC}"
        echo ""

        if is_system_app "$pkg"; then
            echo -e "Status: ${YELLOW}[SYSTEM APP]${NC} — cannot uninstall"
        else
            echo -e "Status: ${GREEN}[USER APP]${NC} — can be uninstalled"
        fi
        echo ""

        echo -e "${CYAN}──────────────────────────────────────────────────────────────────────────────${NC}"
        echo "Actions:"
        echo "  [k] Kill   [s] Starve   [r] Restrict   [d] Deep starve"
        echo "  [u] Uninstall (user only)   [i] Ignore / Skip"
        echo "  [p] Previous   [q] Quit and save"
        echo -n "Your choice: "
        read -r choice

        case "$choice" in
            k|K) action="kill"; destructive=1 ;;
            s|S) action="starve"; destructive=1 ;;
            r|R) action="restrict"; destructive=1 ;;
            d|D) action="deep_starve"; destructive=1 ;;
            u|U) action="uninstall"; destructive=1 ;;
            i|I) action="ignore"; destructive=0 ;;
            p|P)
                if [[ $index -gt 0 ]]; then ((index--)); else echo -e "\n${YELLOW}Already at first package.${NC}"; sleep 1; fi
                continue
                ;;
            q|Q) break ;;
            *) echo -e "\n${RED}Invalid.${NC}"; sleep 1; continue ;;
        esac

        # Confirmation for destructive actions on non-Safe packages
        if [[ $destructive -eq 1 ]] && [[ "$risk" != "Safe" ]]; then
            if ! confirm_action "$pkg" "$action" "$risk_text"; then
                echo -e "\n${CYAN}Cancelled.${NC}"; sleep 1; continue
            fi
        fi

        # Apply action
        case "$action" in
            kill) run_shell "am force-stop $pkg" 2>/dev/null ;;
            starve) run_shell "pm disable-user --user 0 $pkg" 2>/dev/null ;;
            restrict) revoke_dangerous_perms "$pkg" ;;
            deep_starve) deep_starve_pkg "$pkg" ;;
            uninstall) run_shell "pm uninstall --user 0 $pkg" 2>/dev/null ;;
            ignore) ;;
        esac

        db_log_action "PeruseControl" "$pkg" "$action"
        echo -e "\n${GREEN}✅ $pkg → $action${NC}"
        sleep 1

        # Auto‑advance
        if [[ $index -lt $((total-1)) ]]; then
            ((index++))
        else
            visited_last=1
            break
        fi
    done

    if [[ $visited_last -eq 1 ]]; then
        touch "$COMPLETION_MARKER"
        echo -e "\n${GREEN}✅ Training completed! RAM Slammer is now unlocked.${NC}"
    fi
    echo ""
    read -n 1 -s -p "Press any key to return to main menu..."
}

# ------------------------------------------------------------
#  Main Menu
# ------------------------------------------------------------
while true; do
    clear
    echo -e "${CYAN}==============================================================================${NC}"
    echo -e "${CYAN}   🔐 PERUSE CONTROL — MIUI System App Oracle${NC}"
    echo -e "${CYAN}==============================================================================${NC}"
    echo ""
    echo "1) 📖 Interactive Teacher (Required for RAM Slammer)"
    echo "2) 🧹 Debloat Safety Net (coming soon)"
    echo "q) Quit"
    echo ""
    read -p "Choice: " choice
    case $choice in
        1) interactive_teacher ;;
        2) echo "Debloat Safety Net — under construction"; sleep 2 ;;
        q|Q) break ;;
        *) echo "Invalid"; sleep 1 ;;
    esac
done

db_log_tool_end "PeruseControl" "0"
echo -e "\n${CYAN}Session ended.${RESET}"
