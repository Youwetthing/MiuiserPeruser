#!/data/data/com.termux/files/usr/bin/bash
source "$(dirname "$0")/lib/tool_backend.sh"
echo "=============================================="
echo "RAM SLAMMER SURVEILLANCE"
echo "=============================================="

set -e

BASE_DIR="data" LOG_DIR="logs" DB="$BASE_DIR/ram_slammer.db" LEGACY_BRAIN="$BASE_DIR/daemonhunter_brain.txt" RESPAWN_LOG="$LOG_DIR/respawn_offenders.log"

mkdir -p "$BASE_DIR" "$LOG_DIR"

-------------------------

Colours

-------------------------

RED='\033[0;31m' YELLOW='\033[1;33m' GREEN='\033[0;32m' BLUE='\033[0;34m' CYAN='\033[0;36m' NC='\033[0m'

-------------------------

ADB wrapper

-------------------------

safe_adb() { adb shell "$@" 2>/dev/null || return 1 }

-------------------------

Init SQLite brain

-------------------------

init_db() { sqlite3 "$DB" <<EOF CREATE TABLE IF NOT EXISTS apps ( pkg TEXT PRIMARY KEY, action TEXT, respawns INTEGER DEFAULT 0, last_seen INTEGER ); EOF }

init_db

-------------------------

Get memory snapshot (stable method)

-------------------------

get_top_apps() { safe_adb "dumpsys activity processes" 2>/dev/null | 
awk ' /Proc #/ {pkg=""; mem=""} /adj=/ {next} /ProcessRecord/ { for (i=1;i<=NF;i++) { if ($i ~ /com./) pkg=$i } } /TOTAL/ && pkg != "" { print pkg } ' | sort | uniq -c | sort -rn | head -15 }

-------------------------

Tier logic

-------------------------

get_tier() { local mem="$1" if (( mem > 500 )); then echo 4 elif (( mem > 250 )); then echo 3 elif (( mem > 120 )); then echo 2 else echo 1 fi }

apply_action() { local pkg="$1" local tier="$2"

case "$tier" in
    4)
        echo -e "${RED}[DEEP] $pkg${NC}"
        safe_adb "am force-stop $pkg"
        safe_adb "pm disable-user --user 0 $pkg"
        ;;
    3)
        echo -e "${YELLOW}[SUSPEND] $pkg${NC}"
        safe_adb "am force-stop $pkg"
        ;;
    2)
        echo -e "${BLUE}[RESTRICT] $pkg${NC}"
        safe_adb "cmd appops set $pkg RUN_IN_BACKGROUND ignore"
        safe_adb "cmd appops set $pkg WAKE_LOCK ignore"
        ;;
    1)
        echo -e "${GREEN}[MONITOR] $pkg${NC}"
        ;;
esac

}

update_db() { local pkg="$1" local action="$2"

sqlite3 "$DB" "

INSERT INTO apps(pkg, action, last_seen) VALUES('$pkg', '$action', strftime('%s','now')) ON CONFLICT(pkg) DO UPDATE SET action='$action', last_seen=strftime('%s','now'); " }

-------------------------

Main loop

-------------------------

iteration=0

while true; do iteration=$((iteration + 1)) clear

echo -e "${CYAN}=== RAM SLAMMER v2 — Scan #$iteration ===${NC}"
echo ""

mapfile -t apps < <(get_top_apps)

if [[ ${#apps[@]} -eq 0 ]]; then
    echo "No active MIUI-heavy processes detected."
    sleep 5
    continue
fi

for line in "${apps[@]}"; do
    count=$(echo "$line" | awk '{print $1}')
    pkg=$(echo "$line" | awk '{print $2}')

    # fake memory estimate scaling
    mem=$((count * 60))
    tier=$(get_tier "$mem")

    echo -e "${CYAN}$pkg${NC} → ${mem}MB (tier $tier)"

    apply_action "$pkg" "$tier"
    update_db "$pkg" "tier_$tier"

done

echo ""
echo "Commands: [q]uit [r]efresh [c]lear-db"
read -t 10 -n 1 cmd || true

case "$cmd" in
    q|Q)
        echo "Exiting RAM Slammer v2"
        break
        ;;
    c|C)
        sqlite3 "$DB" "DELETE FROM apps;"
        echo "DB cleared"
        ;;
esac

done

-------------------------

End log

-------------------------
