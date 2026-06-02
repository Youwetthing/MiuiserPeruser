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
PROGRESS_FILE="data/.peruse_progress"

mkdir -p data logs
touch "$BRAIN"

GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; BLUE='\033[0;34m'; CYAN='\033[0;36m'; NC='\033[0m'

# ------------------------------------------------------------
#  Shell dispatcher
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
#  Knowledge Base — Updated from your latest screenshot
# ------------------------------------------------------------
declare -A PKG_INFO

# DO NOT UNINSTALL (Critical)
PKG_INFO["com.miui.securitycenter"]="Main Security Center & permission controller — core system protection|Critical"
PKG_INFO["com.miui.home"]="Launcher / Home screen — the entire UI shell|Critical"
PKG_INFO["com.miui.powerkeeper"]="Battery & power management service|Critical"
PKG_INFO["com.miui.daemon"]="Background system daemon (many services depend on it)|Critical"
PKG_INFO["com.miui.cloudservice"]="Xiaomi Cloud sync service|Critical"
PKG_INFO["com.miui.cloudbackup"]="Cloud backup framework|Critical"
PKG_INFO["com.miui.core"]="MIUI core framework|Critical"
PKG_INFO["com.miui.system"]="System framework components|Critical"

# BOOTLOOP / WILL BREAK
PKG_INFO["com.miui.bootloader"]="Bootloader verification component|Critical"
PKG_INFO["com.miui.rom"]="ROM framework — disabling will bootloop|Critical"
PKG_INFO["com.miui.settings"]="System Settings app (will break settings menu)|Critical"
PKG_INFO["com.miui.securityadd"]="Additional security components|Critical"

# MUST KEEP FOR XIAOMI SIGNATURE
PKG_INFO["com.xiaomi.mipicks"]="Xiaomi account & signature service|Critical"
PKG_INFO["com.miui.cloud"]="Main Xiaomi Cloud framework|Critical"

# NOT RECOMMENDED (high risk)
PKG_INFO["com.miui.msa.global"]="MSA / Advertising ID service — system-wide ads|Caution"
PKG_INFO["com.miui.bugreport"]="Bug reporting & diagnostics|Caution"
PKG_INFO["com.miui.analytics"]="Telemetry & usage tracking|Caution"
PKG_INFO["com.xiaomi.joyose"]="Joyose game optimizer / ads|Caution"

# DISABLE BUT CAN BE HANDLED BY DAEMONHUNTERD / RAM SLAMMER
PKG_INFO["com.miui.cleanmaster"]="Junk cleaner / optimizer|Caution"
PKG_INFO["com.miui.videodetail"]="Video detail / recommendation service|Caution"
PKG_INFO["com.miui.weather2"]="Weather app & widget service|Caution"
PKG_INFO["com.miui.gallery"]="Gallery & photo manager|Caution"
PKG_INFO["com.miui.player"]="Music player|Caution"
PKG_INFO["com.miui.hybrid"]="Hybrid app framework|Caution"
PKG_INFO["com.miui.notes"]="Notes app|Caution"
PKG_INFO["com.miui.calculator"]="Calculator app|Caution"
PKG_INFO["com.miui.screenrecorder"]="Screen recorder|Caution"
PKG_INFO["com.miui.fm"]="FM Radio|Caution"
PKG_INFO["com.miui.yellowpage"]="Yellow Pages / local services|Caution"
PKG_INFO["com.miui.compass"]="Compass app|Caution"
PKG_INFO["com.miui.backup"]="Local backup service|Caution"
PKG_INFO["com.miui.personalassistant"]="Personal assistant / suggestions|Caution"
PKG_INFO["com.miui.aod"]="Always-On Display service|Caution"
PKG_INFO["com.miui.tsmclient"]="TSM / payment service|Caution"

# SAFE TO DISABLE / UNINSTALL
PKG_INFO["com.miui.contentextension"]="Content extension service|Safe"
PKG_INFO["com.miui.screenrecorder"]="Screen recorder (duplicate entry)|Safe"
PKG_INFO["com.miui.videodetail"]="Video detail (duplicate)|Safe"

# ------------------------------------------------------------
#  Helper: fetch installed packages and build list
# ------------------------------------------------------------
get_installed_pkg_list() {
    local -n result=$1
    result=()
    installed_raw=$(run_shell "pm list packages" 2>/dev/null | sed 's/package://g')
    if [[ -z "$installed_raw" ]]; then
        return 1
    fi
    declare -A installed_map
    while IFS= read -r p; do
        [[ -n "$p" ]] && installed_map["$p"]=1
    done <<< "$installed_raw"

    declare -A candidate_seen
    for pkg in "${!PKG_INFO[@]}"; do candidate_seen["$pkg"]=1; done
    while IFS='|' read -r pkg _; do [[ -n "$pkg" ]] && candidate_seen["$pkg"]=1; done < "$BRAIN"

    for pkg in "${!candidate_seen[@]}"; do
        [[ -n "${installed_map[$pkg]}" ]] && result+=("$pkg")
    done
    return 0
}

# ------------------------------------------------------------
#  Interactive Teacher Mode — with hall pass
# ------------------------------------------------------------
interactive_teacher() {
    clear
    term_width=$(tput cols 2>/dev/null || echo 80)
    (( term_width < 60 )) && term_width=80

    echo -e "${CYAN}$(printf '=%.0s' $(seq 1 $term_width))${NC}"
    echo -e "${CYAN}   📖 INTERACTIVE TEACHER — Learn Before You Burn${NC}"
    echo -e "${CYAN}$(printf '=%.0s' $(seq 1 $term_width))${NC}"
    echo ""

    # Scan installed packages
    echo -n "Scanning installed packages..."
    local pkg_list
    if ! get_installed_pkg_list pkg_list; then
        echo -e "\n${RED}Error: Could not retrieve package list. Check Shizuku/ADB connection.${NC}"
        read -n 1 -s -p "Press any key to return to main menu..."
        return 1
    fi
    total_installed=${#pkg_list[@]}
    echo " done. Found ${total_installed} MIUI packages."

    if [[ $total_installed -eq 0 ]]; then
        echo -e "${RED}No known MIUI packages found on this device. Exiting.${NC}"
        read -n 1 -s -p "Press any key to return to main menu..."
        return 1
    fi

    # Handle completion marker (hall pass)
    if [[ -f "$COMPLETION_MARKER" ]]; then
        echo ""
        echo -e "${GREEN}✅ Training already completed!${NC}"
        echo ""
        echo -e "What would you like to do?"
        echo "  ${CYAN}1${NC}) Review packages (view-only, no actions)"
        echo "  ${YELLOW}2${NC}) Reset and retake training (clear completion)"
        echo "  ${RED}3${NC}) Cancel and return to menu"
        echo ""
        read -p "Choice [1-3]: " hall_pass_choice
        case $hall_pass_choice in
            1) REVIEW_MODE=1 ;;
            2) rm -f "$COMPLETION_MARKER" "$PROGRESS_FILE"; REVIEW_MODE=0 ;;
            3) return 0 ;;
            *) echo -e "${RED}Invalid choice.${NC}"; sleep 1; return 0 ;;
        esac
    else
        REVIEW_MODE=0
    fi

    # Resume from progress if not in review mode
    start_index=0
    if [[ $REVIEW_MODE -eq 0 ]] && [[ -f "$PROGRESS_FILE" ]]; then
        saved_total=$(head -n1 "$PROGRESS_FILE")
        saved_index=$(tail -n1 "$PROGRESS_FILE")
        if [[ "$saved_total" -eq "$total_installed" ]] && [[ "$saved_index" -lt "$total_installed" ]]; then
            echo -e "${YELLOW}Saved progress found: package $((saved_index+1)) of $total_installed.${NC}"
            echo -n "Resume from where you left off? [Y/n]: "
            read -r resume_choice
            if [[ "$resume_choice" =~ ^[Yy]?$|^$ ]]; then
                start_index=$saved_index
                echo -e "${GREEN}Resuming...${NC}"
            else
                echo -e "${CYAN}Starting fresh.${NC}"
                rm -f "$PROGRESS_FILE"
            fi
        else
            echo -e "${YELLOW}Saved progress is outdated. Starting fresh.${NC}"
            rm -f "$PROGRESS_FILE"
        fi
        echo ""
    fi

    total=${#pkg_list[@]}
    index=$start_index
    declare -A actions_taken

    # If review mode, skip destructive flags and confirmations
    if [[ $REVIEW_MODE -eq 1 ]]; then
        echo -e "${CYAN}📋 REVIEW MODE — No actions will be taken.${NC}"
        echo ""
        read -n 1 -s -p "Press any key to begin review..."
    else
        read -n 1 -s -p "Press any key to begin training..."
    fi

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

        # Risk formatting
        case "$risk" in
            Critical) color="$RED"; risk_text="⛔ CRITICAL"; risk_desc="Do not touch — can cause bootloop or major system failure." ;;
            Caution)  color="$YELLOW"; risk_text="⚠️ CAUTION"; risk_desc="Disabling may cause feature loss — daemonhunterd / RAM Slammer will keep it disabled." ;;
            Safe)     color="$GREEN"; risk_text="✅ SAFE"; risk_desc="Safe to disable, starve, or uninstall." ;;
            *)        color="$CYAN"; risk_text="ℹ️ INFO"; risk_desc="No specific risk level — review carefully." ;;
        esac

        # Progress bar
        bar_width=$((term_width - 20))
        filled=$(( (index+1) * bar_width / total ))
        empty=$((bar_width - filled))
        progress_bar="["
        for ((i=0; i<filled; i++)); do progress_bar+="#"; done
        for ((i=0; i<empty; i++)); do progress_bar+="-"; done
        progress_bar+="] $(( (index+1)*100/total ))%"

        # Header with progress
        echo -e "${CYAN}$(printf '=%.0s' $(seq 1 $term_width))${NC}"
        printf "${CYAN}   Package %d of %d   %s${NC}\n" $((index+1)) $total "$progress_bar"
        echo -e "${CYAN}$(printf '=%.0s' $(seq 1 $term_width))${NC}"
        echo ""

        # Package info box
        box_width=$((term_width - 4))
        echo -e "${color}┌─$(printf '─%.0s' $(seq 1 $box_width))─┐${NC}"
        printf "${color}│ ${BLUE}%-*s${NC} ${color}│${NC}\n" $box_width "$pkg"
        printf "${color}│ ${color}[${risk_text}]${NC} %-$((box_width-${#risk_text}-4))s ${color}│${NC}\n" " "
        echo -e "${color}├─$(printf '─%.0s' $(seq 1 $box_width))─┤${NC}"
        printf "${color}│ ${NC}%-*s${color} │${NC}\n" $box_width "📋 $info"
        printf "${color}│ ${NC}%-*s${color} │${NC}\n" $box_width " "
        printf "${color}│ ${YELLOW}→ %-*s${NC} ${color}│${NC}\n" $((box_width-3)) "$risk_desc"
        echo -e "${color}└─$(printf '─%.0s' $(seq 1 $box_width))─┘${NC}"
        echo ""

        # Status line
        if is_system_app "$pkg"; then
            echo -e "Status: ${YELLOW}[SYSTEM APP]${NC} — cannot be uninstalled, only disabled/restricted."
        else
            echo -e "Status: ${GREEN}[USER APP]${NC} — can be fully uninstalled."
        fi
        echo ""

        if [[ $REVIEW_MODE -eq 1 ]]; then
            # Review mode: limited navigation
            echo -e "${CYAN}─── Review Navigation ─────────────────────────────────────────────${NC}"
            echo ""
            echo -e "  ${BLUE}N${NC}) Next          – Continue to next package"
            echo -e "  ${BLUE}P${NC}) Previous      – Go back to previous package"
            echo -e "  ${RED}Q${NC}) Quit Review   – Return to main menu"
            echo ""
            echo -n "Your choice [N/P/Q]: "
            read -r choice
            case "$choice" in
                n|N|"") 
                    if [[ $index -lt $((total-1)) ]]; then
                        ((index++))
                    else
                        echo -e "\n${GREEN}Review finished!${NC}"
                        sleep 1
                        break
                    fi
                    ;;
                p|P)
                    if [[ $index -gt 0 ]]; then
                        ((index--))
                    else
                        echo -e "\n${YELLOW}Already at first package.${NC}"
                        sleep 1
                    fi
                    ;;
                q|Q) break ;;
                *) echo -e "\n${RED}Invalid choice.${NC}"; sleep 1 ;;
            esac
            continue
        fi

        # Normal mode: full actions
        echo -e "${CYAN}─── Actions ─────────────────────────────────────────────────────${NC}"
        echo ""
        echo -e "  ${GREEN}1${NC}) Kill          – Force stop the app (temporary)"
        echo -e "  ${GREEN}2${NC}) Starve        – Disable the package (persists until re-enabled)"
        echo -e "  ${GREEN}3${NC}) Restrict      – Revoke dangerous permissions only"
        echo -e "  ${GREEN}4${NC}) Deep starve   – Full disable + clear data + restrict all ops"
        if ! is_system_app "$pkg"; then
            echo -e "  ${GREEN}5${NC}) Uninstall     – Remove completely (user apps only)"
        fi
        echo -e "  ${YELLOW}6${NC}) Skip          – Take no action, move to next"
        echo -e "  ${BLUE}P${NC}) Previous      – Go back to previous package"
        echo -e "  ${RED}Q${NC}) Quit & Save   – Exit and save progress"
        echo ""
        echo -n "Your choice [1-6/P/Q]: "
        read -r choice

        case "$choice" in
            1) action="kill"; destructive=1 ;;
            2) action="starve"; destructive=1 ;;
            3) action="restrict"; destructive=1 ;;
            4) action="deep_starve"; destructive=1 ;;
            5) if ! is_system_app "$pkg"; then action="uninstall"; destructive=1; else echo -e "\n${RED}Invalid: cannot uninstall system app.${NC}"; sleep 1; continue; fi ;;
            6) action="ignore"; destructive=0 ;;
            p|P)
                if [[ $index -gt 0 ]]; then
                    ((index--))
                else
                    echo -e "\n${YELLOW}Already at the first package.${NC}"
                    sleep 1
                fi
                continue
                ;;
            q|Q)
                echo "$total" > "$PROGRESS_FILE"
                echo "$index" >> "$PROGRESS_FILE"
                echo -e "\n${GREEN}Progress saved at package $((index+1)) of $total.${NC}"
                sleep 1
                break
                ;;
            *) echo -e "\n${RED}Invalid choice.${NC}"; sleep 1; continue ;;
        esac

        # Confirmation for destructive actions on non-safe packages
        if [[ $destructive -eq 1 ]] && [[ "$risk" != "Safe" ]]; then
            echo ""
            echo -e "${RED}⚠️  CONFIRMATION REQUIRED${NC}"
            echo -e "${RED}   You are about to ${action^^} ${pkg}${NC}"
            echo -e "${RED}   Risk level: ${risk_text}${NC}"
            echo -n "Type 'yes' to proceed, anything else to cancel: "
            read -r confirm
            if [[ "$confirm" != "yes" ]]; then
                echo -e "\n${CYAN}Action cancelled.${NC}"
                sleep 1
                continue
            fi
        fi

        # Execute the action
        case "$action" in
            kill)         run_shell "am force-stop $pkg" 2>/dev/null ;;
            starve)       run_shell "pm disable-user --user 0 $pkg" 2>/dev/null ;;
            restrict)     revoke_dangerous_perms "$pkg" ;;
            deep_starve)  deep_starve_pkg "$pkg" ;;
            uninstall)    run_shell "pm uninstall --user 0 $pkg" 2>/dev/null ;;
            ignore)       ;;
        esac

        actions_taken["$pkg"]="$action"
        db_log_action "PeruseControl" "$pkg" "$action"
        echo -e "\n${GREEN}✅ ${pkg} → ${action}${NC}"
        sleep 1

        # Move to next or finish
        if [[ $index -lt $((total-1)) ]]; then
            ((index++))
        else
            # Completion reached
            rm -f "$PROGRESS_FILE"
            touch "$COMPLETION_MARKER"
            clear
            echo -e "${GREEN}$(printf '=%.0s' $(seq 1 $term_width))${NC}"
            echo -e "${GREEN}   🎉 TRAINING COMPLETE!${NC}"
            echo -e "${GREEN}$(printf '=%.0s' $(seq 1 $term_width))${NC}"
            echo ""
            echo -e "You've reviewed all ${total} packages."
            echo -e "RAM Slammer is now ${GREEN}UNLOCKED${NC}."
            echo ""
            if [[ ${#actions_taken[@]} -gt 0 ]]; then
                echo -e "${CYAN}Summary of actions taken:${NC}"
                for p in "${!actions_taken[@]}"; do
                    printf "  • %-50s → %s\n" "$p" "${actions_taken[$p]}"
                done
            fi
            echo ""
            read -n 1 -s -p "Press any key to return to main menu..."
            break
        fi
    done
}

# ------------------------------------------------------------
#  Reset training progress (hall pass shortcut)
# ------------------------------------------------------------
reset_training() {
    clear
    echo -e "${YELLOW}Reset Training Progress${NC}"
    echo ""
    if [[ -f "$COMPLETION_MARKER" ]] || [[ -f "$PROGRESS_FILE" ]]; then
        echo "This will clear your completion status and any saved progress."
        echo -n "Are you sure? [y/N]: "
        read -r confirm
        if [[ "$confirm" =~ ^[Yy]$ ]]; then
            rm -f "$COMPLETION_MARKER" "$PROGRESS_FILE"
            echo -e "${GREEN}Training progress reset. You can start fresh.${NC}"
        else
            echo "Cancelled."
        fi
    else
        echo -e "${CYAN}No saved progress to reset.${NC}"
    fi
    sleep 2
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
    echo "3) 🔄 Reset training progress (hall pass)"
    echo "q) Quit"
    echo ""
    read -p "Choice: " choice
    case $choice in
        1) interactive_teacher ;;
        2) echo "Debloat Safety Net — under construction"; sleep 2 ;;
        3) reset_training ;;
        q|Q) break ;;
        *) echo "Invalid"; sleep 1 ;;
    esac
done

db_log_tool_end "PeruseControl" "0"
echo -e "\n${CYAN}Session ended.${NC}"
