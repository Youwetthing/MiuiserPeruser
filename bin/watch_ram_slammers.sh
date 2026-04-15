#!/data/data/com.termux/files/usr/bin/bash

BRAIN="data/daemonhunter_brain.txt"
RESPAWN_LOG="logs/respawn_offenders.log"
mkdir -p data logs

# --- Peruse Control Completion Check ---
GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; BLUE='\033[0;34m'; CYAN='\033[0;36m'; NC='\033[0m'
COMPLETION_MARKER="data/.peruse_completed"
if [[ ! -f "$COMPLETION_MARKER" ]]; then
    echo -e "${RED}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${RED}║  🛑 ACCESS DENIED — Peruse Control training required first    ║${NC}"
    echo -e "${RED}╚══════════════════════════════════════════════════════════════╝${NC}"
    echo ""
    echo -e "One person's trash is another person's treasure."
    echo -e "Before using RAM Slammer, you must complete the Peruse Control"
    echo -e "training to understand what each package does."
    echo ""
    echo -e "Run: ${CYAN}./bin/peruse_control.sh${NC}"
    echo ""
    exit 1
fi

touch "$BRAIN" "$RESPAWN_LOG"


declare -A killed_at respawn_count brain_action

while IFS='|' read -r pkg action extra; do
    brain_action["$pkg"]="$action"
done < "$BRAIN"

echo "=============================================="
echo "   🐏 RAM SLAMMER SURVEILLANCE v7 🐏"
echo "=============================================="
echo "Watching MIUI memory hogs every 3 minutes."
echo ""
echo "Commands:"
echo "  [k]ill <pkg>       - force-stop"
echo "  [f]reeze <pkg>     - suspend only"
echo "  [s]tarve <pkg>     - disable"
echo "  [r]estrict <pkg>   - revoke dangerous perms"
echo "  [d]eep starve <pkg>- freeze+disable+appops+OOM"
echo "  [n]etwork <pkg>    - show data usage history"
echo "  [t]cpdump <pkg>    - live capture (requires tcpdump)"
echo "  [q]uit"
echo ""

get_top_ram() {
    local tmp_ram="data/tmp/watch_ram_$$.txt"
    adb shell "dumpsys meminfo 2>/dev/null" > "$tmp_ram"
    # Extract lines like: "  123,456K: com.miui.home (pid 12345 / activities)"
    grep -E '^ *[0-9,]+\+?K: (com\.miui|com\.xiaomi)' "$tmp_ram" 2>/dev/null | \
        while read line; do
            # Extract KB value (remove commas and trailing 'K')
            kb=$(echo "$line" | awk '{print $1}' | tr -d ',K:')
            # Extract package name (the word after "K: " and before " (pid")
            pkg=$(echo "$line" | sed -n 's/.*K: \([^ (]*\).*/\1/p')
            if [[ -n "$kb" && -n "$pkg" ]]; then
                echo "$kb $pkg"
            fi
        done | sort -rn | awk '!seen[$2]++' | head -15 | \
        while read kb pkg; do
            mb=$((kb / 1024))
            echo "$mb $pkg"
        done
    rm -f "$tmp_ram"
}

set_oom_max() {
    local pkg="$1"
    local pids=$(adb shell "pgrep -f $pkg" 2>/dev/null)
    if [[ -n "$pids" ]]; then
        for pid in $pids; do
            adb shell "echo 1000 > /proc/$pid/oom_score_adj" 2>/dev/null
        done
        echo "     🎯 OOM score set to 1000 for $pkg"
    fi
}

iteration=0
while true; do
    iteration=$((iteration + 1))
    clear
    echo "=== Scan #$iteration — $(date '+%H:%M:%S') ==="
    echo ""

    mapfile -t ram_lines < <(get_top_ram)
    
    if [[ ${#ram_lines[@]} -eq 0 ]]; then
        echo "No MIUI packages consuming RAM right now."
    else
        echo "🔥 Current Top RAM Slammers:"
        echo "──────────────────────────────────────────────"
        for line in "${ram_lines[@]}"; do
            mb=$(echo "$line" | awk '{print $1}')
            pkg=$(echo "$line" | awk '{print $2}')
            [[ $mb -gt 150 ]] && color="$RED" || [[ $mb -gt 80 ]] && color="$YELLOW" || color="$NC"
            printf "${color}%4d MB${NC}  %s" "$mb" "$pkg"
            
            if [[ -n "${killed_at[$pkg]}" ]]; then
                respawn_count[$pkg]=$(( ${respawn_count[$pkg]:-0} + 1 ))
                echo -e "  ${RED}⚠️ RESPAWN #${respawn_count[$pkg]}${NC}"
            else
                echo ""
            fi
            
            if [[ -n "${brain_action[$pkg]}" ]]; then
                echo "         └─ Brain says: ${brain_action[$pkg]}"
            fi
        done
    fi

    echo ""
    echo "──────────────────────────────────────────────"
    echo "Commands: [k]ill [f]reeze [s]tarve [r]estrict [d]eep [n]etwork [t]cpdump [q]uit"
    echo -n "> "
    
    read -r cmd rest
    if [[ -n "$cmd" ]]; then
        case "$cmd" in
            q|Q) echo "Exiting. Brain and logs saved."; break ;;
            k|K) pkg="$rest"; adb shell am force-stop "$pkg" 2>/dev/null; echo "✅ Killed $pkg"; killed_at["$pkg"]=$(date +%s) ;;
            f|F) pkg="$rest"; adb shell pm suspend "$pkg" 2>/dev/null; echo "❄️ Frozen $pkg"; brain_action["$pkg"]="freeze"; echo "$pkg|freeze|" >> "$BRAIN"; set_oom_max "$pkg" ;;
            s|S) pkg="$rest"; adb shell pm disable-user --user 0 "$pkg" 2>/dev/null; echo "🔒 Starved $pkg"; brain_action["$pkg"]="starve"; echo "$pkg|starve|" >> "$BRAIN"; set_oom_max "$pkg" ;;
            r|R) pkg="$rest"; for op in 27 26 4 6 14 1; do adb shell cmd appops set "$pkg" "$op" ignore 2>/dev/null; done; echo "🔐 Restricted $pkg"; brain_action["$pkg"]="restrict"; echo "$pkg|restrict|" >> "$BRAIN" ;;
            d|D)
                pkg="$rest"; base_pkg="${pkg%%:*}"
                echo "🔥 Deep starving $pkg..."
                adb shell pm suspend "$base_pkg" 2>/dev/null && echo "     ❄️ Suspended"
                adb shell pm disable-user --user 0 "$base_pkg" 2>/dev/null && echo "     🔒 Disabled"
                adb shell am force-stop "$pkg" 2>/dev/null
                adb shell pm clear "$base_pkg" 2>/dev/null && echo "     🧹 Data cleared"
                for op in 63 64 40 59 27 26 4 6 14 1; do adb shell cmd appops set "$pkg" "$op" ignore 2>/dev/null; done
                echo "     🔐 AppOps locked"
                set_oom_max "$pkg"
                adb shell am force-stop "$pkg" 2>/dev/null
                echo "💀 Deep starved $pkg"
                brain_action["$pkg"]="deep_starve"
                echo "$pkg|deep_starve|" >> "$BRAIN"
                ;;
            n|N)
                pkg="$rest"
                echo "📊 Data usage for $pkg (since last boot):"
                uid=$(adb shell pm list packages -U 2>/dev/null | grep "$pkg" | cut -d' ' -f2 | cut -d: -f2)
                if [[ -z "$uid" ]]; then
                    echo "   Could not determine UID."
                else
                    adb shell dumpsys netstats detail 2>/dev/null | awk -v u="$uid" '
                        $0 ~ "uid=" u { found=1 }
                        found && /networkId/ { print }
                        found && /rb=/ { print "  RX: " $0; found=0 }
                    ' | head -10
                fi
                ;;
            t|T)
                pkg="$rest"
                echo "🌐 Live TCP capture for $pkg..."
                TCPDUMP_CMD=""
                if adb shell test -f /system/bin/tcpdump; then
                    TCPDUMP_CMD="/system/bin/tcpdump"
                elif adb shell test -f /data/local/tmp/tcpdump; then
                    TCPDUMP_CMD="/data/local/tmp/tcpdump"
                else
                    echo "   tcpdump not found. Download with:"
                    echo "   curl -L -o tcpdump https://github.com/AsenOsenov/tcpdump-android-static/raw/main/tcpdump-arm64"
                    echo "   adb push tcpdump /data/local/tmp/ && adb shell chmod 755 /data/local/tmp/tcpdump"
                fi
                if [[ -n "$TCPDUMP_CMD" ]]; then
                    echo "   Capturing 10 packets... (Ctrl+C to stop)"
                    adb shell "$TCPDUMP_CMD -i any -c 10 -n 2>/dev/null" | grep -E "IP|TCP|UDP"
                fi
                ;;
            *) echo "Unknown command." ; sleep 2 ;;
        esac
    fi

    echo ""
    echo "Next scan in 3 minutes... (press any key to skip wait)"
    read -t 180 -n 1 -s
done

echo "=== Respawn Report $(date) ===" >> "$RESPAWN_LOG"
for pkg in "${!respawn_count[@]}"; do
    echo "$pkg respawned ${respawn_count[$pkg]} times" >> "$RESPAWN_LOG"
done
echo "" >> "$RESPAWN_LOG"
echo "Watch session ended. Respawn log: $RESPAWN_LOG"
