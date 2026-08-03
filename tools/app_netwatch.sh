#!/data/data/com.termux/files/usr/bin/bash
# app_netwatch.sh — Per-App Network Connection Monitor
# Maps active connections to packages via UID
# Leith, Patras & Liu (TCD & Univ. Edinburgh, 2021)

BASE="$HOME/MiuiserPeruser"
ADB="/data/data/com.termux/files/home/.cargo/bin/adb_cli"

CYN='\033[96m'; YEL='\033[93m'; RED='\033[91m'
WHT='\033[97m'; DIM='\033[2m'; BOLD='\033[1m'; RST='\033[0m'

adb_run() { $ADB tcp 127.0.0.1:5555 shell "$1" 2>/dev/null; }

# Known suspicious destinations
flag_ip() {
    local ip=$1
    case "$ip" in
        31.13.*|157.240.*|2a03:2880:*) echo "FACEBOOK" ;;
        20.33.*|20.34.*|20.35.*|20.36.*|52.1*) echo "MICROSOFT-AZURE" ;;
        180.163.*|203.119.*|47.246.*)  echo "ALIBABA/XIAOMI" ;;
        120.92.*|123.56.*|39.96.*)     echo "XIAOMI-CN" ;;
        2001:4860:*|8.8.*|172.217.*)   echo "GOOGLE" ;;
        209.85.*)                       echo "GOOGLE-GCM" ;;
        2600:1901:*)                    echo "GOOGLE-CLOUD" ;;
        *)                              echo "" ;;
    esac
}

uid_to_package() {
    local uid=$1
    adb_run "dumpsys package 2>/dev/null | grep -B5 'userId=$uid' | grep 'Package \[' | head -1" \
        | grep -oP "Package \[\K[^\]]+"
}

clear
printf "${BOLD}${WHT}"
printf "╔══════════════════════════════════════════════════╗\n"
printf "║  🌐  APP NETWATCH                                ║\n"
printf "║  Per-app network connection monitor              ║\n"
printf "╚══════════════════════════════════════════════════╝\n"
printf "${RST}\n"

printf "${CYN}Scanning active connections...${RST}\n\n"

# Get all TCP6 connections with UIDs
raw=$(adb_run "cat /proc/net/tcp6 2>/dev/null")
ss_out=$(adb_run "ss -tnp 2>/dev/null")

printf "${BOLD}${WHT}── ACTIVE CONNECTIONS ─────────────────────────────${RST}\n\n"
printf "${DIM}%-20s %-25s %-15s %-10s %s${RST}\n" "REMOTE IP" "PORT" "UID" "PACKAGE" "FLAG"
printf "${DIM}%s${RST}\n" "$(printf '%80s' '' | tr ' ' '-')"

# Parse /proc/net/tcp6 
echo "$raw" | tail -n +2 | while IFS= read -r line; do
    # Extract remote address and uid
    local_hex=$(echo "$line" | awk '{print $2}')
    remote_hex=$(echo "$line" | awk '{print $3}')
    state=$(echo "$line" | awk '{print $4}')
    uid=$(echo "$line" | awk '{print $8}')
    
    # Only ESTABLISHED (01) connections
    [ "$state" != "01" ] && continue
    [ "$uid" = "0" ] && continue
    
    # Decode remote IP (last 4 bytes for IPv4-mapped, full for IPv6)
    remote_port_hex=$(echo "$remote_hex" | cut -d: -f2)
    remote_port=$(printf "%d" "0x${remote_port_hex}" 2>/dev/null)
    
    pkg=$(uid_to_package "$uid")
    [ -z "$pkg" ] && pkg="uid:$uid"
    
    flag=$(flag_ip "$remote_hex")
    
    if [ -n "$flag" ]; then
        printf "${RED}%-20s %-25s %-15s %-10s %s${RST}\n" \
            "${remote_hex:0:20}" ":$remote_port" "$uid" "${pkg:0:20}" "⚠ $flag"
    else
        printf "${WHT}%-20s %-25s %-15s %-10s %s${RST}\n" \
            "${remote_hex:0:20}" ":$remote_port" "$uid" "${pkg:0:20}" ""
    fi
done

printf "\n${BOLD}${WHT}── HUMAN READABLE (ss) ────────────────────────────${RST}\n\n"

# Parse ss output for readable IPs
echo "$ss_out" | grep -v "127.0.0.1:5555\|127.0.0.1:55\|LISTEN" | \
grep -E "ESTAB|CLOSE" | while IFS= read -r line; do
    remote=$(echo "$line" | awk '{print $5}')
    state=$(echo "$line" | awk '{print $1}')
    
    # Extract IP
    remote_ip=$(echo "$remote" | sed 's/\[//;s/\]//' | rev | cut -d: -f2- | rev)
    remote_port=$(echo "$remote" | rev | cut -d: -f1 | rev)
    
    flag=$(flag_ip "$remote_ip")
    
    if [ -n "$flag" ]; then
        printf "${RED}${BOLD}⚠ %-45s :%-6s %s${RST}\n" "$remote_ip" "$remote_port" "$flag"
    else
        printf "${DIM}  %-45s :%-6s${RST}\n" "$remote_ip" "$remote_port"
    fi
done

printf "\n${BOLD}${WHT}── NETSTATS SESSION COUNTS ────────────────────────${RST}\n\n"
adb_run "dumpsys netstats | grep 'uid=.*package='" | while IFS= read -r line; do
    pkg=$(echo "$line" | grep -oP "package=\K[^}]+")
    sessions=$(echo "$line" | grep -oP "}=\K[0-9]+")
    uid=$(echo "$line" | grep -oP "uid=\K[0-9]+")
    
    # Flag Xiaomi packages
    if echo "$pkg" | grep -qE "miui|xiaomi"; then
        printf "${YEL}⚑ %-40s uid=%-8s sessions=%s${RST}\n" "$pkg" "$uid" "$sessions"
    else
        printf "  ${WHT}%-40s${RST} ${DIM}uid=%-8s sessions=%s${RST}\n" "$pkg" "$uid" "$sessions"
    fi
done

printf "\n${CYN}Press any key to exit...${RST}\n"
read -r -n1
