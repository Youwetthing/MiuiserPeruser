#!/data/data/com.termux/files/usr/bin/bash
# =====================================================================
# _id_checker.sh — Identity Layer of MiuiserPeruser
# Maintains UID → Package Map, Search, Reverse Lookup, UID Tools
# =====================================================================

DATA_DIR="$HOME/MiuiserPeruser/data"
UID_MAP="$DATA_DIR/uidpackagemap.txt"
mkdir -p "$DATA_DIR"

# Colours
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# ---------------------------------------------------------------
# 1) Generate Static UID Map
# ---------------------------------------------------------------
generate_map() {
    echo -e "${YELLOW}[*] Generating fresh static UID map...${NC}"
    pm list packages -U \
        | sed 's/package://g' \
        | awk -F ' uid:' '{print $2 " " $1}' \
        | sort -n > "$UID_MAP"

    echo -e "${GREEN}[+] UID map saved to:${NC} $UID_MAP"
    sleep 1
}

# ---------------------------------------------------------------
# 2) Search Package → UID
# ---------------------------------------------------------------
search_package() {
    read -p "Enter package name (partial ok): " pkg
    echo -e "${CYAN}Search results:${NC}"
    grep "$pkg" "$UID_MAP" | awk '{print "UID: " $1 " | PKG: " $2}' \
        || echo "No matches found."
    read -n 1 -s -r -p "Press any key to continue..."
}

# ---------------------------------------------------------------
# 3) Reverse Lookup UID → Package
# ---------------------------------------------------------------
reverse_lookup() {
    read -p "Enter UID: " uid
    echo -e "${CYAN}Results:${NC}"
    grep "^$uid " "$UID_MAP" \
        || echo "No package found for UID $uid"
    read -n 1 -s -r -p "Press any key to continue..."
}

# ---------------------------------------------------------------
# 4) Live Active UID Map (Top Talkers)
# ---------------------------------------------------------------
live_map() {
    echo -e "${YELLOW}Top Active UIDs (Process Count):${NC}"
    ps -Ao uid \
        | sort \
        | uniq -c \
        | sort -nr \
        | head -n 15 \
        | while read count uid; do
            pkg=$(grep "^$uid " "$UID_MAP" | awk '{print $2}')
            [ -z "$pkg" ] && pkg="system/unknown"
            printf "${GREEN}%-8s${NC} | ${CYAN}UID %-6s${NC} | %s\n" "$count" "$uid" "$pkg"
        done
    read -n 1 -s -r -p "Press any key to continue..."
}

# ---------------------------------------------------------------
# 5) RVX Pager (View Full Map)
# ---------------------------------------------------------------
rvx_pager() {
    if [ -f "$UID_MAP" ]; then
        less -R "$UID_MAP"
    else
        echo -e "${RED}[!] No UID map found. Generate one first.${NC}"
        sleep 1
    fi
}

# ---------------------------------------------------------------
# 6) UID Tools Submenu
# ---------------------------------------------------------------
uid_tools() {
    while true; do
        clear
        echo -e "${BLUE}=== UID Tools ===${NC}"
        echo "1) Show current shell UID"
        echo "2) Show system UIDs (0–2000)"
        echo "3) Show app UIDs (10000+)"
        echo "b) Back"
        read -p "Selection: " u

        case $u in
            1) id; read -n 1 ;;
            2) awk '$1 <= 2000' "$UID_MAP"; read -n 1 ;;
            3) awk '$1 >= 10000' "$UID_MAP"; read -n 1 ;;
            b) break ;;
        esac
    done
}

# ---------------------------------------------------------------
# Main Menu
# ---------------------------------------------------------------
while true; do
    clear
    echo -e "${GREEN}MIUISER PERUSER | _id_checker.sh (UID Brain)${NC}"
    echo "--------------------------------------------------"
    [ ! -f "$UID_MAP" ] && echo -e "${RED}[!] UID map missing — run option 1.${NC}"
    echo -e "1) ${YELLOW}Generate Static UID Map${NC}"
    echo "2) Search Package → UID"
    echo "3) Reverse Lookup UID → Package"
    echo "4) Live Active UID Map"
    echo "5) View Full UID Map (Pager)"
    echo "6) UID Tools Submenu"
    echo "q) Quit"
    echo "--------------------------------------------------"
    read -p "Choice: " c

    case $c in
        1) generate_map ;;
        2) search_package ;;
        3) reverse_lookup ;;
        4) live_map ;;
        5) rvx_pager ;;
        6) uid_tools ;;
        q) exit 0 ;;
        *) echo "Invalid choice"; sleep 1 ;;
    esac
done
