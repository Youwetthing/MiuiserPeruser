#!/data/data/com.termux/files/usr/bin/bash
source "$(dirname "$0")/lib/tool_backend.sh"

# -------------------------------------------------------------------
# ------------------------------------------------------------
#  Fully Interpreted Data Panels (Human‑First)
# ------------------------------------------------------------




#  Hidden Settings Explorer — MIUI Quick Access + Debug + Sentry
# -------------------------------------------------------------------
# ------------------------------------------------------------
#  Fully Interpreted Data Panels (Human‑First)
# ------------------------------------------------------------




export PATH="$PATH:$HOME/.shizuku:$PREFIX/bin"

# Colors
BOLD='\033[1m'; DIM='\033[2m'; RESET='\033[0m'
C_BLUE='\033[38;5;39m'; C_GREEN='\033[38;5;48m'; C_YELLOW='\033[38;5;221m'
C_ORANGE='\033[38;5;214m'; C_RED='\033[38;5;203m'; C_PURPLE='\033[38;5;141m'
C_GRAY='\033[38;5;245m'; C_WHITE='\033[38;5;255m'; C_CYAN='\033[0;36m'

# ------------------------------------------------------------
# ------------------------------------------------------------
#  Fully Interpreted Data Panels (Human‑First)
# ------------------------------------------------------------




#  Settings Intents
# ------------------------------------------------------------
# ------------------------------------------------------------
#  Fully Interpreted Data Panels (Human‑First)
# ------------------------------------------------------------




declare -A SETTINGS_INTENTS=(
    ["Developer Options"]="android.settings.APPLICATION_DEVELOPMENT_SETTINGS"
    ["Battery Usage"]="android.intent.action.POWER_USAGE_SUMMARY"
    ["Display Settings"]="android.settings.DISPLAY_SETTINGS"
    ["WiFi Settings"]="android.settings.WIFI_SETTINGS"
    ["Bluetooth Settings"]="android.settings.BLUETOOTH_SETTINGS"
    ["App Permissions"]="android.intent.action.MANAGE_APP_PERMISSIONS"
    ["Default Apps"]="android.settings.MANAGE_DEFAULT_APPS_SETTINGS"
    ["Accessibility"]="android.settings.ACCESSIBILITY_SETTINGS"
    ["Date & Time"]="android.settings.DATE_SETTINGS"
    ["Language & Input"]="android.settings.LOCALE_SETTINGS"
    ["About Phone"]="android.settings.DEVICE_INFO_SETTINGS"
    ["Storage"]="android.settings.INTERNAL_STORAGE_SETTINGS"
    ["Apps"]="android.settings.APPLICATION_SETTINGS"
    ["Location"]="android.settings.LOCATION_SOURCE_SETTINGS"
    ["Security"]="android.settings.SECURITY_SETTINGS"
)

SECRET_CODES=(
    "*#*#6484#*#*|CIT Hardware Test"
    "*#*#4636#*#*|Phone Testing Menu"
    "*#*#34971539#*#*|Camera Firmware Info"
    "*#*#197328640#*#*|Service Mode"
    "*#*#0*#*#*|LCD Test"
    "*#*#2664#*#*|Touchscreen Test"
    "*#*#0283#*#*|Audio Loopback Test"
    "*#*#0842#*#*|Vibration Test"
    "*#*#232338#*#*|WiFi MAC Address"
    "*#*#1472365#*#*|GPS Test"
    "*#*#1111#*#*|FTA Software Version"
    "*#*#2222#*#*|FTA Hardware Version"
    "*#*#1234#*#*|Firmware Version"
    "*#*#86583#*#*|VoLTE Debug"
    "*#*#64663#*#*|Hardware Test (Redmi)"
)

launch_intent() {
    run_shell "am start -a $1" 2>/dev/null
    echo -e "${C_GREEN}✅ Launched${RESET}"
    sleep 1
}

show_codes() {
    clear
    echo -e "${C_BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RESET}"
    echo -e "${C_BLUE}   Secret Dialer Codes (Manual Dial)${RESET}"
    echo ""
    echo -e "${C_YELLOW}⚠️  NOTE: These codes have not worked on EU devices since Android 10/11.${RESET}"
    echo -e "${C_YELLOW}   Google and Xiaomi have blocked most diagnostic codes for security reasons.${RESET}"
    echo -e "${C_YELLOW}   On Android 12+ and HyperOS, they are almost entirely non‑functional.${RESET}"
    echo ""
    echo -e "${C_BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RESET}"
    echo ""
    for entry in "${SECRET_CODES[@]}"; do
        IFS='|' read -r code name <<< "$entry"
        printf "${C_WHITE}%-25s${RESET} : ${C_YELLOW}%s${RESET}\n" "$name" "$code"
    done
    echo ""
    echo -e "${C_GRAY}To use: Open Phone app, paste or type the code, and press Call.${RESET}"
    read -n 1 -s -p "Press any key to return..."
}

sentry_panel() {
    while true; do
        clear
        echo -e "${C_RED}╔══════════════════════════════════════════════════════════════════════════╗${RESET}"
        echo -e "${C_RED}║${RESET} ${BOLD}${C_WHITE}🛡️ SENTRY PANEL — Security Lockdown${RESET}                                    ${C_RED}║${RESET}"
        echo -e "${C_RED}╚══════════════════════════════════════════════════════════════════════════╝${RESET}"
        echo ""
        echo -e "${C_ORANGE}[1] Disable ADB USB    [2] Enable ADB USB    [3] Hide Dev Options${RESET}"
        echo -e "${C_ORANGE}[4] Disable ADB WiFi   [5] Enable ADB WiFi   [6] Show Dev Options${RESET}"
        echo -e "${C_YELLOW}[7] Block Unknown Sources   [8] Allow Unknown Sources   [9] Block OEM Unlock${RESET}"
        echo -e "${C_GREEN}[10] Sensors OFF   [11] Sensors ON   [12] Restrict Background Data${RESET}"
        echo -e "${C_CYAN}[13] Airplane ON   [14] Airplane OFF   [15] Toggle Airplane${RESET}"
        echo "[q] Back"
        read -p "Choice: " c
        case $c in
            1) run_shell "settings put global adb_enabled 0"; echo "✅ ADB USB disabled"; sleep 1 ;;
            2) run_shell "settings put global adb_enabled 1"; echo "✅ ADB USB enabled"; sleep 1 ;;
            3) run_shell "settings put global development_settings_enabled 0"; echo "✅ Dev options hidden"; sleep 1 ;;
            4) run_shell "settings put global adb_wifi_enabled 0"; echo "✅ ADB WiFi disabled"; sleep 1 ;;
            5) run_shell "settings put global adb_wifi_enabled 1"; echo "✅ ADB WiFi enabled"; sleep 1 ;;
            6) run_shell "settings put global development_settings_enabled 1"; echo "✅ Dev options shown"; sleep 1 ;;
            7) run_shell "settings put secure install_non_market_apps 0"; echo "✅ Unknown sources blocked"; sleep 1 ;;
            8) run_shell "settings put secure install_non_market_apps 1"; echo "✅ Unknown sources allowed"; sleep 1 ;;
            9) run_shell "settings put global oem_unlock_enabled 0"; echo "✅ OEM unlock blocked"; sleep 1 ;;
            10) run_shell "settings put system sensors_off 1"; echo "✅ Sensors OFF"; sleep 1 ;;
            11) run_shell "settings put system sensors_off 0"; echo "✅ Sensors ON"; sleep 1 ;;
            12) run_shell "cmd netpolicy set restrict-background true"; echo "✅ Background data restricted"; sleep 1 ;;
            13) run_shell "settings put global airplane_mode_on 1; am broadcast -a android.intent.action.AIRPLANE_MODE"; echo "✅ Airplane ON"; sleep 1 ;;
            14) run_shell "settings put global airplane_mode_on 0; am broadcast -a android.intent.action.AIRPLANE_MODE"; echo "✅ Airplane OFF"; sleep 1 ;;
            15) cur=$(run_shell "settings get global airplane_mode_on" | tr -d '\r\n')
                if [[ "$cur" == "1" ]]; then
                    run_shell "settings put global airplane_mode_on 0"; echo "✅ Airplane OFF"
                else
                    run_shell "settings put global airplane_mode_on 1"; echo "✅ Airplane ON"
                fi
                run_shell "am broadcast -a android.intent.action.AIRPLANE_MODE"; sleep 1 ;;
            q|Q) break ;;
            *) echo "Invalid"; sleep 1 ;;
        esac
    done
}


debug_panel() {
    while true; do
        clear
        echo -e "${C_BLUE}╔══════════════════════════════════════════════════════════════════════════╗${RESET}"
        echo -e "${C_BLUE}║${RESET} ${BOLD}${C_WHITE}🔧 DEBUG PANEL${RESET}                                                           ${C_BLUE}║${RESET}"
        echo -e "${C_BLUE}╚══════════════════════════════════════════════════════════════════════════╝${RESET}"
        echo ""
        echo -e "${C_PURPLE}[a1] Revoke CAMERA from MIUI apps    [a2] Revoke MIC from MIUI apps${RESET}"
        echo -e "${C_PURPLE}[a3] Revoke LOCATION from MIUI apps  [a4] Revoke ALL dangerous perms${RESET}"
        echo ""
        echo -e "${C_ORANGE}[b1] Whitelist Termux   [b2] Whitelist Shizuku   [b3] View whitelist${RESET}"
        echo -e "${C_ORANGE}[b4] Disable Powerkeeper   [b5] Enable Powerkeeper${RESET}"
        echo ""
        echo -e "${C_CYAN}[d1] Dump battery stats   [d2] Running MIUI services   [d3] Wakelocks${RESET}"
        echo -e "${C_CYAN}[d4] App Network Connections (IP → App Name)${RESET}"
        echo -e "${C_CYAN}[d5] Flush DNS              [d6] 🛡️ Sentry Panel${RESET}"
        echo ""
        echo -e "${C_YELLOW}[t1] Disable MIUI Optimizations   [t2] Enable MIUI Optimizations${RESET}"
        echo -e "${C_YELLOW}[t3] Disable Game Turbo           [t4] Disable Memory Extension${RESET}"
        echo ""
        echo -e "[q] Back"
        read -p "Choice: " c
        case $c in
            a1) run_shell "pm list packages" | sed 's/package://' | grep -E "com\.(miui|xiaomi)" | while read p; do run_shell "cmd appops set $p 26 ignore"; done; echo "✅ Camera revoked"; sleep 1 ;;
            a2) run_shell "pm list packages" | sed 's/package://' | grep -E "com\.(miui|xiaomi)" | while read p; do run_shell "cmd appops set $p 27 ignore"; done; echo "✅ Mic revoked"; sleep 1 ;;
            a3) run_shell "pm list packages" | sed 's/package://' | grep -E "com\.(miui|xiaomi)" | while read p; do run_shell "cmd appops set $p 1 ignore"; done; echo "✅ Location revoked"; sleep 1 ;;
            a4) for op in 26 27 1 4; do run_shell "pm list packages" | sed 's/package://' | grep -E "com\.(miui|xiaomi)" | while read p; do run_shell "cmd appops set $p $op ignore"; done; done; echo "✅ All revoked"; sleep 2 ;;
            b1) run_shell "dumpsys deviceidle whitelist +com.termux"; echo "✅ Termux whitelisted"; sleep 1 ;;
            b2) run_shell "dumpsys deviceidle whitelist +moe.shizuku.privileged.api"; echo "✅ Shizuku whitelisted"; sleep 1 ;;
            b3) run_shell "dumpsys deviceidle whitelist" | grep -E "termux|shizuku"; read -n1 -s ;;
            b4) run_shell "pm disable-user --user 0 com.miui.powerkeeper"; echo "✅ Powerkeeper disabled"; sleep 1 ;;
            b5) run_shell "pm enable com.miui.powerkeeper"; echo "✅ Powerkeeper enabled"; sleep 1 ;;
            d1) smart_battery_stats ;;
            d2) smart_running_services ;;
            d3) smart_wakelocks ;;
            d4) show_app_connections ;;
            d5) run_shell "ndc resolver flushdefaultif"; echo "✅ DNS flushed"; sleep 1 ;;
            d6) sentry_panel ;;
            t1) run_shell "settings put secure miui_optimization 0"; echo "✅ MIUI Optimizations disabled"; sleep 1 ;;
            t2) run_shell "settings put secure miui_optimization 1"; echo "✅ MIUI Optimizations enabled"; sleep 1 ;;
            t3) run_shell "settings put secure game_turbo_enabled 0"; echo "✅ Game Turbo disabled"; sleep 1 ;;
            t4) run_shell "settings put global memory_extension_enabled 0"; echo "✅ Memory Extension disabled"; sleep 1 ;;
            q|Q) break ;;
            *) echo "Invalid"; sleep 1 ;;
        esac
    done
}

# Connection test and main menu
clear
echo -e "${C_BLUE}╔══════════════════════════════════════════════════════════════════════════╗${RESET}"
echo -e "${C_BLUE}║${RESET} ${BOLD}${C_WHITE}🔓 HIDDEN SETTINGS EXPLORER${RESET}                                                  ${C_BLUE}║${RESET}"
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
    echo -e "${C_BLUE}║${RESET} ${BOLD}${C_WHITE}🔓 HIDDEN SETTINGS EXPLORER${RESET}                                                  ${C_BLUE}║${RESET}"
    echo -e "${C_BLUE}╚══════════════════════════════════════════════════════════════════════════╝${RESET}"
    echo ""
    echo -e "${C_PURPLE}┌─── QUICK ACCESS (Settings Intents) ──────────────────────────────────────┐${RESET}"
    echo -e "${C_PURPLE}│${RESET} [1] Developer Options     [2] Battery Usage        [3] Display Settings ${C_PURPLE}│${RESET}"
    echo -e "${C_PURPLE}│${RESET} [4] WiFi Settings         [5] Bluetooth Settings   [6] App Permissions   ${C_PURPLE}│${RESET}"
    echo -e "${C_PURPLE}│${RESET} [7] Default Apps          [8] Accessibility        [9] Date & Time       ${C_PURPLE}│${RESET}"
    echo -e "${C_PURPLE}│${RESET} [10] Language & Input     [11] About Phone         [12] Storage          ${C_PURPLE}│${RESET}"
    echo -e "${C_PURPLE}│${RESET} [13] Apps                 [14] Location            [15] Security         ${C_PURPLE}│${RESET}"
    echo -e "${C_PURPLE}└───────────────────────────────────────────────────────────────────────────┘${RESET}"
    echo ""
    echo -e "${C_CYAN}[16] 📞 Secret Dialer Codes (Manual)${RESET}"
    echo -e "${C_CYAN}[17] 🔧 Debug Panel${RESET}"
    echo -e "${C_CYAN}[18] 🛡️ Sentry Panel${RESET}"
    echo -e "${C_CYAN}[q] Quit${RESET}"
    echo ""
    read -p "Choice: " choice
    case $choice in
        1) launch_intent "${SETTINGS_INTENTS["Developer Options"]}" ;;
        2) launch_intent "${SETTINGS_INTENTS["Battery Usage"]}" ;;
        3) launch_intent "${SETTINGS_INTENTS["Display Settings"]}" ;;
        4) launch_intent "${SETTINGS_INTENTS["WiFi Settings"]}" ;;
        5) launch_intent "${SETTINGS_INTENTS["Bluetooth Settings"]}" ;;
        6) launch_intent "${SETTINGS_INTENTS["App Permissions"]}" ;;
        7) launch_intent "${SETTINGS_INTENTS["Default Apps"]}" ;;
        8) launch_intent "${SETTINGS_INTENTS["Accessibility"]}" ;;
        9) launch_intent "${SETTINGS_INTENTS["Date & Time"]}" ;;
        10) launch_intent "${SETTINGS_INTENTS["Language & Input"]}" ;;
        11) launch_intent "${SETTINGS_INTENTS["About Phone"]}" ;;
        12) launch_intent "${SETTINGS_INTENTS["Storage"]}" ;;
        13) launch_intent "${SETTINGS_INTENTS["Apps"]}" ;;
        14) launch_intent "${SETTINGS_INTENTS["Location"]}" ;;
        15) launch_intent "${SETTINGS_INTENTS["Security"]}" ;;
        16) show_codes ;;
        17) debug_panel ;;
        18) sentry_panel ;;
        q|Q) break ;;
        *) echo "Invalid"; sleep 1 ;;
    esac
done

echo -e "\n${C_CYAN}Session ended.${RESET}"

# ------------------------------------------------------------
# ------------------------------------------------------------
#  Fully Interpreted Data Panels (Human‑First)
# ------------------------------------------------------------




#  Smart Dumpsys Interpreters
# ------------------------------------------------------------
# ------------------------------------------------------------
#  Fully Interpreted Data Panels (Human‑First)
# ------------------------------------------------------------







# Override the old d1, d2, d3 with the smart versions
