#!/data/data/com.termux/files/usr/bin/bash
source "$(dirname "$0")/lib/tool_backend.sh"

# -------------------------------------------------------------------
#  MIUI Update Blocker — Prevent Forced OTAs
# -------------------------------------------------------------------
export PATH="$PATH:$HOME/.shizuku:$PREFIX/bin"

# Colors
BOLD='\033[1m'; DIM='\033[2m'; RESET='\033[0m'
C_RED='\033[38;5;203m'; C_GREEN='\033[38;5;48m'; C_YELLOW='\033[38;5;221m'
C_BLUE='\033[38;5;39m'; C_CYAN='\033[0;36m'; C_WHITE='\033[38;5;255m'
C_GRAY='\033[38;5;245m'

# ------------------------------------------------------------
#  Shell Dispatcher
# ------------------------------------------------------------

# ------------------------------------------------------------
#  OTA-related packages and services
# ------------------------------------------------------------
OTA_PACKAGES=(
    "com.miui.securitycenter"
    "com.miui.cloudservice"
    "com.miui.cloudbackup"
    "com.xiaomi.xmsf"
    "com.xiaomi.finddevice"
    "com.android.updater"
    "com.miui.systemAdSolution"
)

OTA_SERVICES=(
    "com.miui.cloudservice/.CloudService"
    "com.xiaomi.xmsf/.XmsfService"
)

# OTA server domains to block via hosts
OTA_DOMAINS=(
    "update.miui.com"
    "update.intl.miui.com"
    "ota.xiaomi.com"
    "sdkconfig.xiaomi.com"
    "tracking.xiaomi.com"
    "api.sec.miui.com"
)

HOSTS_FILE="/system/etc/hosts"
BACKUP_HOSTS="data/hosts.backup"

# ------------------------------------------------------------
#  Check current status
# ------------------------------------------------------------
check_status() {
    echo -e "${C_BLUE}┌─── CURRENT STATUS ──────────────────────────────────────────────────────┐${RESET}"
    
    # Check packages
    for pkg in "${OTA_PACKAGES[@]}"; do
        state=$(run_shell "pm list packages -d" 2>/dev/null | grep -c "$pkg")
        if [[ $state -gt 0 ]]; then
            printf "${C_BLUE}│${RESET} %-40s : ${C_RED}DISABLED${RESET}\n" "$pkg"
        else
            installed=$(run_shell "pm list packages" 2>/dev/null | grep -c "$pkg")
            if [[ $installed -gt 0 ]]; then
                printf "${C_BLUE}│${RESET} %-40s : ${C_GREEN}ENABLED${RESET}\n" "$pkg"
            else
                printf "${C_BLUE}│${RESET} %-40s : ${C_GRAY}NOT INSTALLED${RESET}\n" "$pkg"
            fi
        fi
    done
    
    # Check hosts file
    if run_shell "grep -q 'update.miui.com' $HOSTS_FILE" 2>/dev/null; then
        echo -e "${C_BLUE}│${RESET} ${C_WHITE}Hosts file${RESET}              : ${C_RED}BLOCKED${RESET} (OTA domains redirected)"
    else
        echo -e "${C_BLUE}│${RESET} ${C_WHITE}Hosts file${RESET}              : ${C_GREEN}CLEAN${RESET} (no blocks)"
    fi
    
    echo -e "${C_BLUE}└───────────────────────────────────────────────────────────────────────────┘${RESET}"
}

# ------------------------------------------------------------
#  Block OTA (packages + hosts + freeze services)
# ------------------------------------------------------------
block_ota() {
    echo -e "${C_YELLOW}🛑 Blocking OTA updates...${RESET}"
    
    # 1. Disable packages
    for pkg in "${OTA_PACKAGES[@]}"; do
        run_shell "pm disable-user --user 0 $pkg" 2>/dev/null
        echo -e "   ${C_GREEN}✓${RESET} Disabled $pkg"
        db_log_action "UpdateBlocker" "$pkg" "disable"
    done
    
    # 2. Freeze services
    for svc in "${OTA_SERVICES[@]}"; do
        pkg="${svc%%/*}"
        run_shell "pm suspend $pkg" 2>/dev/null
        echo -e "   ${C_GREEN}✓${RESET} Suspended $pkg"
    done
    
    # 3. Block via hosts (requires system write)
    echo -e "${C_GRAY}   Attempting hosts file block...${RESET}"
    run_shell "cp $HOSTS_FILE $BACKUP_HOSTS" 2>/dev/null
    
    # Build hosts entries
    local hosts_entries="\n# MIUI OTA Block (MiuiserPeruser)\n127.0.0.1 ${OTA_DOMAINS[0]}"
    for domain in "${OTA_DOMAINS[@]:1}"; do
        hosts_entries="$hosts_entries $domain"
    done
    hosts_entries="$hosts_entries\n"
    
    # Try to append (requires rw mount)
    if run_shell "mount -o remount,rw /system" 2>/dev/null; then
        run_shell "echo -e \"$hosts_entries\" >> $HOSTS_FILE"
        run_shell "mount -o remount,ro /system" 2>/dev/null
        echo -e "   ${C_GREEN}✓${RESET} Hosts file updated"
    else
        echo -e "   ${C_YELLOW}⚠${RESET} Could not remount /system (hosts block skipped)"
    fi
    
    echo -e "${C_GREEN}✅ OTA updates blocked.${RESET}"
    sleep 2
}

# ------------------------------------------------------------
#  Restore OTA (re-enable everything)
# ------------------------------------------------------------
restore_ota() {
    echo -e "${C_GREEN}🔄 Restoring OTA updates...${RESET}"
    
    # 1. Enable packages
    for pkg in "${OTA_PACKAGES[@]}"; do
        run_shell "pm enable --user 0 $pkg" 2>/dev/null
        echo -e "   ${C_GREEN}✓${RESET} Enabled $pkg"
        db_log_action "UpdateBlocker" "$pkg" "enable"
    done
    
    # 2. Unfreeze services
    for svc in "${OTA_SERVICES[@]}"; do
        pkg="${svc%%/*}"
        run_shell "pm unsuspend $pkg" 2>/dev/null
        echo -e "   ${C_GREEN}✓${RESET} Unsuspended $pkg"
    done
    
    # 3. Restore hosts backup if exists
    if run_shell "test -f $BACKUP_HOSTS" 2>/dev/null; then
        if run_shell "mount -o remount,rw /system" 2>/dev/null; then
            run_shell "cp $BACKUP_HOSTS $HOSTS_FILE"
            run_shell "mount -o remount,ro /system" 2>/dev/null
            echo -e "   ${C_GREEN}✓${RESET} Hosts file restored"
        fi
    fi
    
    echo -e "${C_GREEN}✅ OTA updates restored.${RESET}"
    sleep 2
}

# ------------------------------------------------------------
#  Connection Test & Main Menu
# ------------------------------------------------------------
clear
echo -e "${C_BLUE}╔══════════════════════════════════════════════════════════════════════════╗${RESET}"
echo -e "${C_BLUE}║${RESET} ${BOLD}${C_WHITE}🛑 MIUI UPDATE BLOCKER${RESET}                                                    ${C_BLUE}║${RESET}"
echo -e "${C_BLUE}╚══════════════════════════════════════════════════════════════════════════╝${RESET}"
echo ""

echo -e "${C_CYAN}📡 Connecting...${RESET}"
if command -v adb >/dev/null 2>&1 && adb shell echo ready 2>/dev/null | grep -q ready; then
    echo -e "${C_GREEN}   ✅ Connected via ADB${RESET}"
elif [[ -x "$HOME/rish" ]] && "$HOME/rish" -c "echo ready" 2>/dev/null | grep -q ready; then
    echo -e "${C_GREEN}   ✅ Connected via Shizuku rish${RESET}"
else
    echo -e "${C_RED}   ❌ No connection — ensure ADB or Shizuku is running${RESET}"
    exit 1
fi
sleep 1

while true; do
    clear
    echo -e "${C_BLUE}╔══════════════════════════════════════════════════════════════════════════╗${RESET}"
    echo -e "${C_BLUE}║${RESET} ${BOLD}${C_WHITE}🛑 MIUI UPDATE BLOCKER${RESET}                                                    ${C_BLUE}║${RESET}"
    echo -e "${C_BLUE}╚══════════════════════════════════════════════════════════════════════════╝${RESET}"
    echo ""
    
    check_status
    echo ""
    
    echo -e "${C_RED}[1] BLOCK OTA${RESET} — Disable packages, freeze services, block domains"
    echo -e "${C_GREEN}[2] RESTORE OTA${RESET} — Re-enable everything"
    echo -e "${C_CYAN}[3] Refresh status${RESET}"
    echo -e "[q] Quit"
    echo ""
    read -p "Choice: " choice
    
    case $choice in
        1) block_ota ;;
        2) restore_ota ;;
        3) continue ;;
        q|Q) break ;;
        *) echo "Invalid"; sleep 1 ;;
    esac
done

echo -e "\n${C_CYAN}Session ended.${RESET}"
