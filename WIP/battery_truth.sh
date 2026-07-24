#!/data/data/com.termux/files/usr/bin/bash

# -------------------------------------------------------------------
#  Battery Truth — Live Stats + Xiaomi Reference Database
# -------------------------------------------------------------------

export PATH="$PATH:$HOME/.shizuku:$PREFIX/bin"

db_log_battery() { :; }
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../lib/miuiserperuser_common.sh"
db_log_tool_start "BatteryTruth"

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

ua_to_ma() { local ua="$1"; [[ -n "$ua" && "$ua" =~ ^-?[0-9]+$ ]] && echo $((ua / 1000)) || echo "N/A"; }

# ------------------------------------------------------------
#  Connection Test
# ------------------------------------------------------------
clear
echo -e "${C_BLUE}${BOX_TL_THICK}$(printf '%*s' 76 | tr ' ' "${BOX_H}")${BOX_TR_THICK}${RESET}"
echo -e "${C_BLUE}${BOX_V}${RESET} ${BOLD}${C_WHITE}🔋 BATTERY TRUTH${RESET}"
echo -e "${C_BLUE}${BOX_V}${RESET} ${DIM}${C_GRAY}Live stats + Xiaomi reference database${RESET}"
echo -e "${C_BLUE}${BOX_BL_THICK}$(printf '%*s' 76 | tr ' ' "${BOX_H}")${BOX_BR_THICK}${RESET}"
echo ""

echo -e "${C_CYAN}📡 Connecting to device...${RESET}"

if [[ -x "$HOME/.shizuku/rish" ]] && "$HOME/.shizuku/rish" -c "getprop ro.product.model" 2>/dev/null | grep -q .; then
    echo -e "${C_GREEN}   ✅ Connected via Shizuku rish${RESET}"
elif adb shell getprop ro.product.model 2>/dev/null | grep -q .; then
    echo -e "${C_GREEN}   ✅ Connected via ADB${RESET}"
else
    echo -e "${C_RED}   ❌ No connection.${RESET}"
    exit 1
fi

# ------------------------------------------------------------
#  Friendly Loading Panel
# ------------------------------------------------------------
echo ""
echo -e "${C_PURPLE}┌─── WHAT WE'RE CHECKING ──────────────────────────────────────────────────┐${RESET}"
echo -e "${C_PURPLE}${BOX_V}${RESET}                                                                              ${C_PURPLE}${BOX_V}${RESET}"
echo -e "${C_PURPLE}${BOX_V}${RESET}   ${C_WHITE}Capacity Health${RESET}   • How much of your original battery remains         ${C_PURPLE}${BOX_V}${RESET}"
echo -e "${C_PURPLE}${BOX_V}${RESET}                     ${DIM}${C_GREEN}> 90% = Healthy${RESET}  ${C_YELLOW}80–89% = Aging${RESET}  ${C_RED}< 80% = Degraded${RESET}         ${C_PURPLE}${BOX_V}${RESET}"
echo -e "${C_PURPLE}${BOX_V}${RESET}                                                                              ${C_PURPLE}${BOX_V}${RESET}"
echo -e "${C_PURPLE}${BOX_V}${RESET}   ${C_WHITE}Cycle Count${RESET}      • Number of full charge/discharge cycles               ${C_PURPLE}${BOX_V}${RESET}"
echo -e "${C_PURPLE}${BOX_V}${RESET}                     ${DIM}Xiaomi batteries rated for ~800 cycles before degradation${RESET}  ${C_PURPLE}${BOX_V}${RESET}"
echo -e "${C_PURPLE}${BOX_V}${RESET}                                                                              ${C_PURPLE}${BOX_V}${RESET}"
echo -e "${C_PURPLE}${BOX_V}${RESET}   ${C_WHITE}Charge Current${RESET}   • How fast your battery is charging right now            ${C_PURPLE}${BOX_V}${RESET}"
echo -e "${C_PURPLE}${BOX_V}${RESET}                     ${DIM}⚡ Fast (>2000mA)  🔌 Normal (1000–2000)  🐢 Slow (<1000)${RESET}    ${C_PURPLE}${BOX_V}${RESET}"
echo -e "${C_PURPLE}${BOX_V}${RESET}                                                                              ${C_PURPLE}${BOX_V}${RESET}"
echo -e "${C_PURPLE}${BOX_V}${RESET}   ${C_WHITE}Temperature${RESET}      • Battery heat level                                      ${C_PURPLE}${BOX_V}${RESET}"
echo -e "${C_PURPLE}${BOX_V}${RESET}                     ${DIM}${C_GREEN}< 35°C = Cool${RESET}  ${C_YELLOW}35–43°C = Warm${RESET}  ${C_RED}> 43°C = Hot (throttling)${RESET}      ${C_PURPLE}${BOX_V}${RESET}"
echo -e "${C_PURPLE}${BOX_V}${RESET}                                                                              ${C_PURPLE}${BOX_V}${RESET}"
echo -e "${C_PURPLE}└───────────────────────────────────────────────────────────────────────────┘${RESET}"

echo ""
echo -e "${C_GRAY}⏳ Reading live data from your device...${RESET}"

# ------------------------------------------------------------
#  Live Data Collection
# ------------------------------------------------------------
run_shell dumpsys battery > "$TMP_DIR/battery_dump.txt"
health=$(grep "health:" "$TMP_DIR/battery_dump.txt" | awk '{print $2}')
level=$(grep "level:" "$TMP_DIR/battery_dump.txt" | awk '{print $2}')
status=$(grep "status:" "$TMP_DIR/battery_dump.txt" | awk '{print $2}')
temp_raw=$(grep "temperature:" "$TMP_DIR/battery_dump.txt" | awk '{print $2}')
voltage=$(grep "voltage:" "$TMP_DIR/battery_dump.txt" | awk '{print $2}')
[[ -n "$temp_raw" ]] && temp_c=$((temp_raw / 10))

SYSFS_BASE="/sys/class/power_supply/battery"
run_shell "cat $SYSFS_BASE/cycle_count 2>/dev/null" > "$TMP_DIR/cycle_count.txt"
run_shell "cat $SYSFS_BASE/current_now 2>/dev/null" > "$TMP_DIR/current_now.txt"
run_shell "cat $SYSFS_BASE/temp 2>/dev/null" > "$TMP_DIR/batt_temp.txt"
run_shell "cat $SYSFS_BASE/charge_full_design 2>/dev/null" > "$TMP_DIR/charge_full_design.txt"
run_shell "cat $SYSFS_BASE/charge_full 2>/dev/null" > "$TMP_DIR/charge_full.txt"
run_shell "cat $SYSFS_BASE/manufacturer 2>/dev/null" > "$TMP_DIR/manufacturer.txt"
run_shell "cat $SYSFS_BASE/model_name 2>/dev/null" > "$TMP_DIR/model_name.txt"

# Parse
cycle_count=$(cat "$TMP_DIR/cycle_count.txt" 2>/dev/null | grep -oE '[0-9]+' | head -1)
current_now=$(cat "$TMP_DIR/current_now.txt" 2>/dev/null | grep -oE '[-]?[0-9]+' | head -1)
batt_temp_sys=$(cat "$TMP_DIR/batt_temp.txt" 2>/dev/null | grep -oE '[0-9]+' | head -1)
[[ -n "$batt_temp_sys" ]] && temp_sys_c=$((batt_temp_sys / 10))

design_capacity_sys=$(cat "$TMP_DIR/charge_full_design.txt" 2>/dev/null | grep -oE '[0-9]+' | head -1)
actual_capacity_sys=$(cat "$TMP_DIR/charge_full.txt" 2>/dev/null | grep -oE '[0-9]+' | head -1)
battery_manufacturer=$(cat "$TMP_DIR/manufacturer.txt" 2>/dev/null | tr -d '\r\n')
battery_model=$(cat "$TMP_DIR/model_name.txt" 2>/dev/null | tr -d '\r\n')

[[ -z "$battery_manufacturer" || "$battery_manufacturer" == "Unknown" ]] && battery_manufacturer=$(run_shell getprop ro.product.manufacturer)
[[ -z "$battery_model" || "$battery_model" == "Unknown" ]] && battery_model=$(run_shell getprop ro.product.model)

device_model=$(run_shell getprop ro.product.model)
device_brand=$(run_shell getprop ro.product.brand)

if [[ -n "$design_capacity_sys" && -n "$actual_capacity_sys" && "$design_capacity_sys" -gt 0 ]]; then
    health_pct=$((actual_capacity_sys * 100 / design_capacity_sys))
fi

current_ma=$(ua_to_ma "$current_now")
abs_current=${current_ma#-}
if [[ $abs_current -gt 2000 ]]; then speed_text="⚡ Fast"; speed_color="$C_GREEN"
elif [[ $abs_current -gt 1000 ]]; then speed_text="🔌 Normal"; speed_color="$C_YELLOW"
elif [[ $abs_current -gt 100 ]]; then speed_text="🐢 Slow"; speed_color="$C_ORANGE"
else speed_text="⏸️ Idle"; speed_color="$C_GRAY"; fi

# ------------------------------------------------------------
#  Reference Database Lookup
# ------------------------------------------------------------
REF_DB="data/battery_reference.db"
has_ref=0
if [[ -f "$REF_DB" ]]; then
    ref_result=$(sqlite3 "$REF_DB" "SELECT supplier, capacity_mah, cell_type, android_launch, notes FROM battery_models WHERE model LIKE '%$device_model%' LIMIT 1;" 2>/dev/null)
    if [[ -n "$ref_result" ]]; then
        IFS='|' read -r ref_supplier ref_capacity ref_cell ref_android ref_notes <<< "$ref_result"
        has_ref=1
    fi
fi

# ------------------------------------------------------------
#  Clear and Display Final Report
# ------------------------------------------------------------
clear
echo -e "${C_BLUE}${BOX_TL_THICK}$(printf '%*s' 76 | tr ' ' "${BOX_H}")${BOX_TR_THICK}${RESET}"
echo -e "${C_BLUE}${BOX_V}${RESET} ${BOLD}${C_WHITE}🔋 BATTERY TRUTH — ${device_brand} ${device_model}${RESET}"
echo -e "${C_BLUE}${BOX_V}${RESET} ${DIM}${C_GRAY}Live stats + Xiaomi reference database${RESET}"
echo -e "${C_BLUE}${BOX_BL_THICK}$(printf '%*s' 76 | tr ' ' "${BOX_H}")${BOX_BR_THICK}${RESET}"
echo ""

# Identity
echo -e "${C_PURPLE}┌─── IDENTITY ─────────────────────────────────────────────────────────────┐${RESET}"
printf "${C_PURPLE}${BOX_V}${RESET} ${C_GRAY}%-18s${RESET} : ${C_WHITE}%s${RESET}\n" "Device" "${device_brand} ${device_model}"
printf "${C_PURPLE}${BOX_V}${RESET} ${C_GRAY}%-18s${RESET} : ${C_WHITE}%s${RESET}\n" "Battery Mfr" "${battery_manufacturer:-Unknown}"
printf "${C_PURPLE}${BOX_V}${RESET} ${C_GRAY}%-18s${RESET} : ${C_WHITE}%s${RESET}\n" "Battery Model" "${battery_model:-Unknown}"
printf "${C_PURPLE}${BOX_V}${RESET} ${C_GRAY}%-18s${RESET} : ${C_WHITE}%s mAh${RESET}\n" "Design Capacity" "${design_capacity_sys:-N/A}"
if [[ -n "$cycle_count" ]]; then
    if [[ $cycle_count -lt 400 ]]; then cycle_c="$C_GREEN"; elif [[ $cycle_count -lt 600 ]]; then cycle_c="$C_YELLOW"; else cycle_c="$C_RED"; fi
    printf "${C_PURPLE}${BOX_V}${RESET} ${C_GRAY}%-18s${RESET} : ${cycle_c}%s cycles${RESET}\n" "Cycle Count" "$cycle_count"
fi
echo -e "${C_PURPLE}└───────────────────────────────────────────────────────────────────────────┘${RESET}"
echo ""

# Reference Database Panel
if [[ $has_ref -eq 1 ]]; then
    echo -e "${C_BLUE}┌─── OFFICIAL REFERENCE SPECS ────────────────────────────────────────────┐${RESET}"
    printf "${C_BLUE}${BOX_V}${RESET} ${C_GRAY}%-18s${RESET} : ${C_WHITE}%s${RESET}\n" "Supplier" "${ref_supplier:-N/A}"
    printf "${C_BLUE}${BOX_V}${RESET} ${C_GRAY}%-18s${RESET} : ${C_WHITE}%s mAh${RESET}\n" "Rated Capacity" "${ref_capacity:-N/A}"
    printf "${C_BLUE}${BOX_V}${RESET} ${C_GRAY}%-18s${RESET} : ${C_WHITE}%s${RESET}\n" "Cell Type" "${ref_cell:-N/A}"
    printf "${C_BLUE}${BOX_V}${RESET} ${C_GRAY}%-18s${RESET} : ${C_WHITE}%s${RESET}\n" "Android Launch" "${ref_android:-N/A}"
    if [[ -n "$ref_notes" ]]; then
        printf "${C_BLUE}${BOX_V}${RESET} ${C_YELLOW}📌 %s${RESET}\n" "$ref_notes"
    fi
    echo -e "${C_BLUE}└───────────────────────────────────────────────────────────────────────────┘${RESET}"
    echo ""
fi

# Health
echo -e "${C_BLUE}┌─── LIVE HEALTH ──────────────────────────────────────────────────────────┐${RESET}"
printf "${C_BLUE}${BOX_V}${RESET} ${C_GRAY}%-18s${RESET} : ${C_WHITE}%s${RESET}\n" "Status" "${health:-Unknown}"
printf "${C_BLUE}${BOX_V}${RESET} ${C_GRAY}%-18s${RESET} : ${C_WHITE}%s%%${RESET}\n" "Current Level" "${level:-?}"
printf "${C_BLUE}${BOX_V}${RESET} ${C_GRAY}%-18s${RESET} : ${C_WHITE}%s°C${RESET}\n" "Temperature" "${temp_c:-?}"
if [[ -n "$health_pct" ]]; then
    if [[ $health_pct -ge 90 ]]; then health_c="$C_GREEN"
    elif [[ $health_pct -ge 80 ]]; then health_c="$C_YELLOW"
    else health_c="$C_RED"; fi
    printf "${C_BLUE}${BOX_V}${RESET} ${C_GRAY}%-18s${RESET} : ${health_c}%s%% (%s/%s mAh)${RESET}\n" "Capacity Health" "$health_pct" "${actual_capacity_sys:-?}" "${design_capacity_sys:-?}"
fi
echo -e "${C_BLUE}└───────────────────────────────────────────────────────────────────────────┘${RESET}"
echo ""

# Charging
echo -e "${C_BLUE}┌─── CHARGING ─────────────────────────────────────────────────────────────┐${RESET}"
printf "${C_BLUE}${BOX_V}${RESET} ${C_GRAY}%-18s${RESET} : ${C_WHITE}%s${RESET}\n" "Status" "${status:-Unknown}"
if [[ "$current_ma" != "N/A" ]]; then
    printf "${C_BLUE}${BOX_V}${RESET} ${C_GRAY}%-18s${RESET} : ${speed_color}%s mA (%s)${RESET}\n" "Current" "$current_ma" "$speed_text"
fi
printf "${C_BLUE}${BOX_V}${RESET} ${C_GRAY}%-18s${RESET} : ${C_WHITE}%s mV${RESET}\n" "Voltage" "${voltage:-?}"
echo -e "${C_BLUE}└───────────────────────────────────────────────────────────────────────────┘${RESET}"
echo ""

# System
powerkeeper_active=$(run_shell pgrep -f com.miui.powerkeeper)
echo -e "${C_ORANGE}┌─── SYSTEM ───────────────────────────────────────────────────────────────┐${RESET}"
if [[ -n "$powerkeeper_active" ]]; then
    printf "${C_ORANGE}${BOX_V}${RESET} ${C_RED}⚠️  powerkeeper ACTIVE${RESET}\n"
else
    printf "${C_ORANGE}${BOX_V}${RESET} ${C_GREEN}✅ powerkeeper not running${RESET}\n"
fi
if [[ -n "$batt_temp_sys" && "$batt_temp_sys" -gt 430 ]]; then
    printf "${C_ORANGE}${BOX_V}${RESET} ${C_RED}⚠️  Battery > 43°C — throttling possible${RESET}\n"
else
    printf "${C_ORANGE}${BOX_V}${RESET} ${C_GREEN}✅ No thermal throttling${RESET}\n"
fi
echo -e "${C_ORANGE}└───────────────────────────────────────────────────────────────────────────┘${RESET}"
echo ""

# Care
echo -e "${C_GRAY}${DIM}┌─── XIAOMI BATTERY CARE ──────────────────────────────────────────────────┐${RESET}"
echo -e "${C_GRAY}${DIM}${BOX_V}${RESET} ${DIM}• Ideal temp: 15–28°C | Keep 40–80% charge${RESET}"
echo -e "${C_GRAY}${DIM}${BOX_V}${RESET} ${DIM}• Health < 80%: Consider replacement${RESET}"
echo -e "${C_GRAY}${DIM}${BOX_V}${RESET} ${DIM}• Avoid overnight charging | Use original adapter${RESET}"
echo -e "${C_GRAY}${DIM}└───────────────────────────────────────────────────────────────────────────┘${RESET}"
echo ""

# Logging
if declare -f db_log_battery >/dev/null 2>&1; then
    db_log_battery "${health_pct:-0}" "${cycle_count:-0}" "${temp_c:-0}" "${current_ma:-0}" "${level:-0}" "${status:-Unknown}"
    db_log_tool_end "BatteryTruth" "0"
fi

rm -f "$TMP_DIR"/*.txt
read -n 1 -s -p "Press any key to return..."
