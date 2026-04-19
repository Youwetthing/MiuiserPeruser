#!/data/data/com.termux/files/usr/bin/bash

stty -ixon 2>/dev/null
stty cols 200 2>/dev/null

GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
GREY='\033[0;37m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
NC='\033[0m'

run_shell() {
    adb shell "$@" 2>/dev/null
}

# ----------------------------
# CLASSIFICATION ENGINE
# ----------------------------
classify() {
    local name="$1"

    case "$name" in
        u0_*) echo "APP" ;;
        com.miui.*|miui*) echo "MIUI" ;;
        com.google.*|google*) echo "GOOGLE" ;;
        com.android.*|android.*) echo "SYSTEM" ;;
        *mediatek*|*vendor*|*xiaomi*) echo "VENDOR" ;;
        \[*\]*) echo "KERNEL" ;;
        *) echo "OTHER" ;;
    esac
}

color_for_class() {
    case "$1" in
        APP) echo "$GREEN" ;;
        SYSTEM) echo "$CYAN" ;;
        MIUI) echo "$YELLOW" ;;
        GOOGLE) echo "$BLUE" ;;
        VENDOR) echo "$PURPLE" ;;
        KERNEL) echo "$GREY" ;;
        *) echo "$NC" ;;
    esac
}

# ----------------------------
# CORE PROCESS VIEW
# ----------------------------
process_view() {
    clear
    echo -e "${CYAN}=== PROCESS DASHBOARD ===${NC}"
    echo ""

    run_shell "ps -A -o PID,UID,NAME" | tail -n +2 | while read -r pid uid name; do

        cls=$(classify "$name")
        col=$(color_for_class "$cls")

        printf "${col}%-6s %-6s %-10s %-40s ${NC}\n" \
            "$pid" "$uid" "$cls" "$name"
    done
}

# ----------------------------
# APP ONLY VIEW
# ----------------------------
apps_only() {
    clear
    echo -e "${GREEN}=== USER APPS ONLY ===${NC}"
    echo ""

    run_shell "ps -A -o PID,UID,NAME" | tail -n +2 | while read -r pid uid name; do
        [[ "$uid" == u0_* ]] && \
        printf "${GREEN}%-6s %-10s %s${NC}\n" "$pid" "$uid" "$name"
    done
}

# ----------------------------
# MIUI + BACKGROUND TRACKING
# ----------------------------
miui_view() {
    clear
    echo -e "${YELLOW}=== MIUI / SYSTEM LAYER ===${NC}"
    echo ""

    run_shell "ps -A -o PID,UID,NAME" | tail -n +2 | while read -r pid uid name; do
        case "$name" in
            com.miui.*|miui*|com.xiaomi.*|com.android.systemui*)
                printf "${YELLOW}%-6s %-10s %s${NC}\n" "$pid" "$uid" "$name"
            ;;
        esac
    done
}

# ----------------------------
# TOP VIEW (SORT BY UID)
# ----------------------------
sorted_view() {
    clear
    echo -e "${CYAN}=== SORTED BY UID ===${NC}"
    echo ""

    run_shell "ps -A -o PID,UID,NAME" | tail -n +2 | sort -k2 -n | while read -r pid uid name; do
        cls=$(classify "$name")
        col=$(color_for_class "$cls")

        printf "${col}%-6s %-6s %-10s %s${NC}\n" "$pid" "$uid" "$cls" "$name"
    done
}

# ----------------------------
# MENU
# ----------------------------
while true; do
    echo ""
    echo -e "${CYAN}==== Miuiser Process Engine ====${NC}"
    echo "1) Full process dashboard"
    echo "2) App-only view"
    echo "3) MIUI / system focus"
    echo "4) Sorted UID view"
    echo "0) Exit"

    read -p "Choice: " c

    case "$c" in
        1) process_view ;;
        2) apps_only ;;
        3) miui_view ;;
        4) sorted_view ;;
        0) exit ;;
    esac
done

