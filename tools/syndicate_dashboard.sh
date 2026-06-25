#!/data/data/com.termux/files/usr/bin/bash
# tools/syndicate_dashboard.sh — Pure Single-Column Toggle

BASE="$HOME/MiuiserPeruser"
CONFIG="$BASE/data/syndicate_config.json"
SCAN_SCRIPT="$BASE/scripts/syndicate_scan.sh"

# ── All 12 Daemons ────────────────────────────────────────────────────────────
DAEMONS=(
    "burned"
    "granitord"
    "leatherheadd"
    "metalheadd"
    "rahzerd"
    "ratkingd"
    "rocksteadyd"
    "shredderd"
    "tigerclawd"
    "bebopd"
    "fugitoidd"
    "overlordd"
)

# ── Config Loader ─────────────────────────────────────────────────────────────
load_config() {
    if [ -f "$CONFIG" ]; then
        ENABLED=$(jq -r '.enabled_daemons[]' "$CONFIG" 2>/dev/null | tr '\n' ' ')
        if [ -z "$ENABLED" ]; then
            ENABLED="${DAEMONS[*]}"
        fi
    else
        ENABLED="${DAEMONS[*]}"
        save_config
    fi
}

# ── Config Saver ──────────────────────────────────────────────────────────────
save_config() {
    local json='{"enabled_daemons":['
    local first=1
    for d in $ENABLED; do
        if [ $first -eq 0 ]; then
            json+=','
        fi
        json+="\"$d\""
        first=0
    done
    json+=']}'
    echo "$json" > "$CONFIG"
}

# ── Load on startup ──────────────────────────────────────────────────────────
load_config

# ── Main Loop ──────────────────────────────────────────────────────────────────
while true; do
    clear
    echo -e "\033[36m\033[1m┌──────────────────────────────────────────────┐\033[0m"
    echo -e "\033[36m\033[1m│ SYNDICATE: Daemon Toggle Dashboard          │\033[0m"
    echo -e "\033[36m\033[1m└──────────────────────────────────────────────┘\033[0m"
    echo ""

    # Single-column display (No wrapping on 48-char width)
    idx=1
    for d in "${DAEMONS[@]}"; do
        if [[ " $ENABLED " =~ " $d " ]]; then
            printf " \033[33m%2d\033[0m %-16s \033[32m[ON]\033[0m\n" "$idx" "$d"
        else
            printf " \033[33m%2d\033[0m %-16s \033[31m[OFF]\033[0m\n" "$idx" "$d"
        fi
        idx=$((idx + 1))
    done

    echo ""
    echo -e " \033[33mr\033[0m Run Syndicate Scan (Launches syndicate_scan.sh)"
    echo -e " \033[33mb\033[0m Back to main dashboard"
    echo ""
    echo -n -e " \033[36m> \033[0m"

    read -n1 input

    # ── Toggle Daemon ──────────────────────────────────────────────────────────
    if [[ "$input" =~ ^[0-9]+$ ]] && [ "$input" -ge 1 ] && [ "$input" -le 12 ]; then
        target="${DAEMONS[$((input-1))]}"
        
        if [[ " $ENABLED " =~ " $target " ]]; then
            # Remove from list
            ENABLED=$(echo "$ENABLED" | sed "s/\b$target\b//g" | xargs)
            echo -e "\n\033[31m⛔ $target disabled.\033[0m"
        else
            # Add to list
            ENABLED="$ENABLED $target"
            echo -e "\n\033[32m✅ $target enabled.\033[0m"
        fi
        save_config
        sleep 1

    # ── Run Scan ──────────────────────────────────────────────────────────────
    elif [[ "$input" == "r" ]] || [[ "$input" == "R" ]]; then
        echo -e "\n\033[2mLaunching syndicate_scan.sh with current selection...\033[0m\n"
        if [ -f "$SCAN_SCRIPT" ]; then
            "$SCAN_SCRIPT"
        else
            echo -e "\033[31mError: $SCAN_SCRIPT not found.\033[0m"
            sleep 2
        fi

    # ── Back ──────────────────────────────────────────────────────────────────
    elif [[ "$input" == "b" ]] || [[ "$input" == "B" ]]; then
        exit 0
    fi
done

