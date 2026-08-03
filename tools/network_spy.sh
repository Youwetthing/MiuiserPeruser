#!/data/data/com.termux/files/usr/bin/bash
source "$(dirname "$0")/lib/tool_backend.sh"

# -------------------------------------------------------------------
#  Network Spy — Full UID/Process Network Monitor (Shizuku Required)
# -------------------------------------------------------------------
export PATH="$PATH:$HOME/.shizuku:$PREFIX/bin"

# Colors
C_GREEN='\033[38;5;48m'; C_YELLOW='\033[38;5;221m'; C_RED='\033[38;5;203m'
C_BLUE='\033[38;5;39m'; C_PURPLE='\033[38;5;141m'; C_CYAN='\033[0;36m'
C_GRAY='\033[38;5;245m'; C_WHITE='\033[38;5;255m'; RESET='\033[0m'

# ------------------------------------------------------------
#  Shell Dispatcher (rish first, ADB fallback)
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
#  System UID Friendly Names (extensive)
# ------------------------------------------------------------
declare -A SYSTEM_UIDS=(
    [1000]="Android System"
    [1001]="Phone/Radio"
    [1002]="Bluetooth"
    [1003]="NFC"
    [1004]="Input Devices"
    [1005]="Graphics"
    [1006]="Media"
    [1007]="DRM"
    [1008]="VPN"
    [1009]="WiFi"
    [1010]="WiFi P2P"
    [1011]="Location/GPS"
    [1012]="Sensors"
    [1013]="Media Codec"
    [1014]="OMX"
    [1015]="Camera"
    [1016]="Audio"
    [1017]="MIDI"
    [1018]="USB"
    [1019]="WiFi Aware"
    [1020]="Network Stack"
    [1021]="GPS Daemon"
    [1023]="Media Transcoding"
    [1024]="Stats"
    [1025]="Incident"
    [1026]="Secure Element"
    [1027]="UWB"
    [1032]="Media Provider"
    [1041]="App Ops"
    [1073]="Network Monitor"
    [2000]="ADB Shell"
)

# ------------------------------------------------------------
#  Build UID → Package Map (cached)
# ------------------------------------------------------------
MAP_FILE="data/uid_package_map_full.txt"
mkdir -p data

build_uid_map() {
    echo -e "${C_GRAY}⏳ Building UID map...${RESET}"
    run_shell "pm list packages -U" 2>/dev/null | sed -n 's/package:\(.*\) uid:\(.*\)/\2 \1/p' > "$MAP_FILE"
    echo -e "${C_GREEN}✅ Map built with $(wc -l < "$MAP_FILE") entries${RESET}"
}

if [[ ! -f "$MAP_FILE" ]] || [[ $(find "$MAP_FILE" -mmin +60 2>/dev/null) ]]; then
    build_uid_map
fi

# ------------------------------------------------------------
#  Lookup Name for UID
# ------------------------------------------------------------
lookup_name() {
    local uid="$1"
    local pkg
    
    # Try package map
    pkg=$(grep "^$uid " "$MAP_FILE" 2>/dev/null | head -1 | cut -d' ' -f2)
    if [[ -n "$pkg" ]]; then
        echo "$pkg"
        return
    fi
    
    # Try system UID table
    if [[ -n "${SYSTEM_UIDS[$uid]}" ]]; then
        echo "${SYSTEM_UIDS[$uid]} (UID $uid)"
        return
    fi
    
    # Kernel threads
    if [[ $uid -lt 1000 ]]; then
        echo "Kernel (UID $uid)"
        return
    fi
    
    echo "unknown (UID $uid)"
}

# ------------------------------------------------------------
#  Parse /proc/net/* files
# ------------------------------------------------------------
parse_connections() {
    local proto="$1"
    local file="$2"
    local state_filter="$3"
    
    run_shell "cat $file" 2>/dev/null | while read -r line; do
        [[ "$line" =~ ^[[:space:]]*[0-9]+: ]] || continue
        
        local uid; uid=$(echo "$line" | awk '{print $8}')
        [[ -z "$uid" || "$uid" == "0" ]] && continue
        
        local remote; remote=$(echo "$line" | awk '{print $3}')
        local remote_hex; remote_hex=$(echo "$remote" | cut -d: -f1)
        local port_hex; port_hex=$(echo "$remote" | cut -d: -f2)
        [[ "$remote_hex" =~ ^[0-9A-Fa-f]{8}$ ]] || continue
        
        local ip; ip=$(printf "%d.%d.%d.%d" \
            "0x${remote_hex:6:2}" "0x${remote_hex:4:2}" \
            "0x${remote_hex:2:2}" "0x${remote_hex:0:2}")
        local port=$((0x$port_hex))
        
        [[ "$ip" == "0.0.0.0" || "$ip" == "127.0.0.1" ]] && continue
        
        local name; name=$(lookup_name "$uid")
        
        # State decoding (simplified)
        local state_hex; state_hex=$(echo "$line" | awk '{print $4}')
        local state=""
        case $state_hex in
            01) state="ESTABLISHED" ;;
            02) state="SYN_SENT" ;;
            03) state="SYN_RECV" ;;
            04) state="FIN_WAIT1" ;;
            05) state="FIN_WAIT2" ;;
            06) state="TIME_WAIT" ;;
            07) state="CLOSE" ;;
            08) state="CLOSE_WAIT" ;;
            0A) state="LISTEN" ;;
            0B) state="CLOSING" ;;
            *) state="UNKNOWN" ;;
        esac
        
        # Filter by state if specified
        if [[ -n "$state_filter" && "$state" != "$state_filter" ]]; then
            continue
        fi
        
        printf "${C_GRAY}%-8s${RESET} ${C_WHITE}%-18s${RESET} : %-6s ${C_YELLOW}%-40s${RESET} ${C_CYAN}%s${RESET}\n" \
            "$proto" "$ip" "$port" "$name" "$state"
    done
}

# ------------------------------------------------------------
#  Main Display
# ------------------------------------------------------------
clear
echo -e "${C_BLUE}╔══════════════════════════════════════════════════════════════════════════╗${RESET}"
echo -e "${C_BLUE}║${RESET} ${BOLD}${C_WHITE}🕵️ NETWORK SPY — Full UID Connection Monitor${RESET}                         ${C_BLUE}║${RESET}"
echo -e "${C_BLUE}╚══════════════════════════════════════════════════════════════════════════╝${RESET}"
echo ""

# Options
echo -e "${C_PURPLE}Show:${RESET}"
echo "  [1] All connections (TCP + UDP)"
echo "  [2] Established TCP only"
echo "  [3] Listening ports only"
echo "  [4] UDP only"
echo "  [5] Rebuild UID map (force refresh)"
echo "  [q] Quit"
echo ""
read -r -p "Choice: " choice

case $choice in
    1)
        clear
        echo -e "${C_CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RESET}"
        echo -e "${C_CYAN}   All TCP/UDP Connections${RESET}"
        echo -e "${C_CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RESET}"
        parse_connections "TCP" "/proc/net/tcp"
        parse_connections "TCP6" "/proc/net/tcp6"
        parse_connections "UDP" "/proc/net/udp"
        parse_connections "UDP6" "/proc/net/udp6"
        ;;
    2)
        clear
        echo -e "${C_CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RESET}"
        echo -e "${C_CYAN}   Established TCP Connections${RESET}"
        echo -e "${C_CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RESET}"
        parse_connections "TCP" "/proc/net/tcp" "ESTABLISHED"
        parse_connections "TCP6" "/proc/net/tcp6" "ESTABLISHED"
        ;;
    3)
        clear
        echo -e "${C_CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RESET}"
        echo -e "${C_CYAN}   Listening Ports${RESET}"
        echo -e "${C_CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RESET}"
        parse_connections "TCP" "/proc/net/tcp" "LISTEN"
        parse_connections "TCP6" "/proc/net/tcp6" "LISTEN"
        ;;
    4)
        clear
        echo -e "${C_CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RESET}"
        echo -e "${C_CYAN}   UDP Connections${RESET}"
        echo -e "${C_CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RESET}"
        parse_connections "UDP" "/proc/net/udp"
        parse_connections "UDP6" "/proc/net/udp6"
        ;;
    5)
        build_uid_map
        ;;
    q|Q)
        exit 0
        ;;
    *)
        echo "Invalid"
        ;;
esac

echo ""
read -r -n 1 -s -p "Press any key to return..."
