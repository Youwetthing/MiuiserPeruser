#!/data/data/com.termux/files/usr/bin/bash
# miuiser_sar.sh — GDPR Subject Access Request Tool
# Interactive wrapper for sar_engine

BASE="$HOME/MiuiserPeruser"
SAR="$BASE/bin/sar_engine"

CYN='\033[96m'; YEL='\033[93m'; RED='\033[91m'; GRN='\033[32m'
WHT='\033[97m'; DIM='\033[2m'; BOLD='\033[1m'; RST='\033[0m'

clear
printf "${BOLD}${WHT}"
printf "╔══════════════════════════════════════════════════╗\n"
printf "║  ⚖  MIUISERPERUSER — PRIVACY & DATA RIGHTS      ║\n"
printf "║  GDPR Subject Access Request                     ║\n"
printf "╚══════════════════════════════════════════════════╝\n"
printf "${RST}\n"
printf "${DIM}  GDPR Article 2(2)(c) — Personal research exemption\n"
printf "  Data subject = Data controller = You\n"
printf "  All data stays on this device.${RST}\n\n"

printf "  ${YEL}[1]${RST} ${WHT}View data inventory${RST}\n"
printf "  ${YEL}[2]${RST} ${WHT}Export all my data (SAR)${RST}\n"
printf "  ${YEL}[3]${RST} ${WHT}Purge specific data${RST}\n"
printf "  ${YEL}[4]${RST} ${WHT}Purge everything${RST}\n"
printf "  ${YEL}[5]${RST} ${WHT}View consent records${RST}\n"
printf "  ${YEL}[6]${RST} ${WHT}Revoke consent${RST}\n"
printf "  ${YEL}[q]${RST} ${WHT}Quit${RST}\n\n"

read -r -p "$(printf "  ${CYN}Choice: ${RST}")" choice

case "$choice" in
    1) $SAR --inventory ;;
    2)
        ts=$(date '+%Y%m%d_%H%M%S')
        out="/sdcard/MiuiserPeruser_SAR_${ts}.tar.gz"
        $SAR --export --out "$out"
        ;;
    3)
        printf "\n  ${WHT}Categories: daemon_results, logs, baselines, consent${RST}\n"
        read -r -p "$(printf "  ${CYN}Category: ${RST}")" cat
        $SAR --purge --category "$cat"
        ;;
    4) $SAR --purge --category all ;;
    5) $SAR --consent --list ;;
    6)
        printf "\n  ${WHT}Subsystems: syndicate, superhero, all${RST}\n"
        read -r -p "$(printf "  ${CYN}Revoke: ${RST}")" sub
        $SAR --consent --revoke "$sub"
        ;;
    q|Q) exit 0 ;;
esac

printf "\n  ${DIM}Press any key...${RST}"
read -n1
