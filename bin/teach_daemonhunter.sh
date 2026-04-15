#!/data/data/com.termux/files/usr/bin/bash

BRAIN="data/daemonhunter_brain.txt"
LOG="logs/daemonhunter.log"
TMP_DIR="data/tmp"
mkdir -p "$TMP_DIR" data logs
TMP_RAM="$TMP_DIR/miui_ram_$$.txt"

touch "$BRAIN" 2>/dev/null

GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; BLUE='\033[0;34m'; NC='\033[0m'

cleanup() { rm -f "$TMP_RAM"; }
trap cleanup EXIT

echo "=============================================="
echo "     Daemon Hunter — Smart Teaching v6"
echo "=============================================="
echo "Waiting for full dumpsys meminfo (may take 15-20s)..."
echo ""

# 1. Fetch memory info with retry logic
MAX_WAIT=30
elapsed=0
while [[ $elapsed -lt $MAX_WAIT ]]; do
    adb shell "dumpsys meminfo 2>/dev/null" > "$TMP_RAM"
    # Check if we got per-process lines (format: "  123,456K: com.miui.xxx")
    if grep -qE '^ *[0-9,]+\+?K: com\.(miui|xiaomi)' "$TMP_RAM"; then
        echo "✅ Memory snapshot captured ($(wc -l < "$TMP_RAM") lines)"
        break
    fi
    echo -n "."
    sleep 2
    elapsed=$((elapsed + 2))
done

if [[ $elapsed -ge $MAX_WAIT ]]; then
    echo -e "\n${YELLOW}⚠️ dumpsys didn't return per-process data. Using top fallback...${NC}"
    adb shell "top -n 1 -b 2>/dev/null" > "$TMP_RAM"
    TOP_FALLBACK=1
else
    TOP_FALLBACK=0
fi

# 2. Package list
mapfile -t packages < <(adb shell "pm list packages 2>/dev/null | grep -E 'com.miui|com.xiaomi' | sed 's/package://g' | sort")

# 3. System/user classification
declare -A is_system
while read -r line; do
    pkg=$(echo "$line" | sed 's/package://')
    is_system["$pkg"]=1
done < <(adb shell "pm list packages -s 2>/dev/null | grep -E 'com.miui|com.xiaomi'")

# 4. Load existing brain
declare -A already_taught
while IFS='|' read -r pkg action extra; do
    already_taught["$pkg"]="$action|$extra"
done < "$BRAIN"

# Helper: RAM in KB (fixed pattern)
get_ram_kb() {
    local pkg="$1"
    if [[ $TOP_FALLBACK -eq 1 ]]; then
        grep "$pkg" "$TMP_RAM" 2>/dev/null | head -1 | awk '{print $6}' | sed 's/M//;s/K//;s/G//'
    else
        # Extract number before "K:" for the specific package
        grep "com\.miui\.\|com\.xiaomi\." "$TMP_RAM" 2>/dev/null | grep "$pkg" | head -1 | sed -n 's/^ *\([0-9,]\+\)K:.*/\1/p' | tr -d ','
    fi
}

# Helper: dangerous permissions list
get_dangerous_perms() {
    local pkg="$1"
    adb shell dumpsys package "$pkg" 2>/dev/null | grep -A100 "granted=true" | grep -oE "android.permission.[A-Z_]+" | grep -E "READ_SMS|RECORD_AUDIO|ACCESS_FINE_LOCATION|CAMERA|READ_CONTACTS|READ_CALL_LOG|WRITE_EXTERNAL_STORAGE" | sed 's/android.permission.//' | sort -u
}

# Helper: revoke permission via appops
revoke_perm() {
    local pkg="$1"
    local perm="$2"
    case "$perm" in
        RECORD_AUDIO) op=27 ;;
        CAMERA) op=26 ;;
        READ_CONTACTS) op=4 ;;
        READ_CALL_LOG) op=6 ;;
        READ_SMS) op=14 ;;
        ACCESS_FINE_LOCATION) op=1 ;;
        WRITE_EXTERNAL_STORAGE) op=60 ;;
        *) return ;;
    esac
    adb shell cmd appops set "$pkg" "$op" ignore 2>/dev/null
}

# 5. Teaching loop
total=${#packages[@]}
count=0
for pkg in "${packages[@]}"; do
    ((count++))
    if [[ -n "${already_taught[$pkg]}" ]]; then
        continue
    fi

    ram_kb=$(get_ram_kb "$pkg")
    ram_kb=$(echo "$ram_kb" | grep -oE '[0-9]+' | head -1)
    if [[ -n "$ram_kb" && "$ram_kb" -gt 0 ]]; then
        ram_mb=$((ram_kb / 1024))
    else
        ram_mb=0
    fi

    mapfile -t perms_array < <(get_dangerous_perms "$pkg")
    perms_str=$(echo "${perms_array[*]}" | tr '\n' ',' | sed 's/,$//')
    [[ -z "$perms_str" ]] && perms_str="None"

    sys_flag="${is_system[$pkg]}"
    if [[ "$sys_flag" == "1" ]]; then
        sys_status="${YELLOW}[SYSTEM]${NC}"
        can_uninstall="no"
    else
        sys_status="${GREEN}[USER]${NC}"
        can_uninstall="yes"
    fi

    attention=""
    if [[ $ram_mb -gt 150 ]]; then
        attention="${RED}⚠️ HIGH RAM (${ram_mb} MB)${NC}  "
    elif [[ $ram_mb -gt 80 ]]; then
        attention="${YELLOW}⚠️ Moderate RAM (${ram_mb} MB)${NC}  "
    else
        attention="RAM: ${ram_mb} MB  "
    fi

    if [[ "$perms_str" != "None" ]]; then
        attention+="${RED}⚠️ Dangerous: ${perms_str}${NC}"
    fi

    clear
    echo "Package $count of $total"
    echo "──────────────────────────────────────────────"
    echo -e "Package: ${BLUE}$pkg${NC}  $sys_status"
    echo -e "$attention"
    echo ""
    echo "Actions:"
    echo "  [k] Kill (force-stop)"
    echo "  [s] Starve (disable package)"
    echo "  [r] Restrict (revoke dangerous permissions via AppOps)"
    echo "  [u] Uninstall (user apps only)"
    echo "  [i] Ignore"
    echo "  [q] Quit"
    echo -n "Your choice: "
    read -r choice

    extra=""
    case "$choice" in
        k|K)
            action="kill"
            adb shell am force-stop "$pkg" 2>/dev/null
            ;;
        s|S)
            action="starve"
            adb shell pm disable-user --user 0 "$pkg" 2>/dev/null
            ;;
        r|R)
            action="restrict"
            for perm in "${perms_array[@]}"; do
                revoke_perm "$pkg" "$perm"
                echo "  Revoked: $perm"
            done
            extra="$perms_str"
            ;;
        u|U)
            if [[ "$can_uninstall" == "yes" ]]; then
                action="uninstall"
                adb shell pm uninstall --user 0 "$pkg" 2>/dev/null
            else
                echo -e "${RED}Cannot uninstall system package.${NC}"
                read -p "Press Enter..."
                continue
            fi
            ;;
        i|I) action="ignore" ;;
        q|Q) echo "Saving and quitting..."; break ;;
        *) echo "Invalid"; read -p "Press Enter..."; continue ;;
    esac

    echo "$pkg|$action|$extra" >> "$BRAIN"
    echo "[$(date)] $action: $pkg $extra" >> "$LOG"
    echo -e "${GREEN}✅ $pkg -> $action${NC}"
    sleep 1
done

echo ""
echo "=============================================="
echo "Teaching complete. Brain: $BRAIN"
echo "=============================================="
awk -F'|' '{print "  " $2 ": " $1 ($3 ? " (" $3 ")" : "")}' "$BRAIN" | sort
echo ""
read -n 1 -s -p "Press any key to return..."
