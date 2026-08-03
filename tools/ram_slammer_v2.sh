#!/data/data/com.termux/files/usr/bin/bash
# ==============================================================================
#  Ram Slammer v2.2 — Memory & Process Behavioural Analysis
#  Fixed: RAM_STATE, tier math, rish preflight, memdata parsing
#  Companion to: ratkingd, leatherheadd, Don
# ==============================================================================

BASE="${HOME}/MiuiserPeruser"
DB="${BASE}/data/ram_slammer.db"
LAST_SCAN="${BASE}/data/last_scan.json"
RATKINGD_JSON="${BASE}/Registry/daemon_results/ratkingd.json"
LEATHERHEADD_JSON="${BASE}/Registry/daemon_results/leatherheadd.json"
RAM_STATE="${BASE}/pipes/ram_state"

export RISH_APPLICATION_ID=com.termux

# ── Palette ───────────────────────────────────────────────────────────────────
ST='\033[38;5;244m'   # steel
NG='\033[38;5;82m'    # neon green
AM='\033[38;5;214m'   # amber
RD='\033[38;5;196m'   # red alert
CY='\033[38;5;51m'    # cyan
WH='\033[38;5;255m'   # white
DK='\033[38;5;235m'   # dark
BOLD='\033[1m'
RESET='\033[0m'

COLS=$(tput cols 2>/dev/null || echo 60)
W=$(( COLS - 2 )); [ "$W" -gt 76 ] && W=76

RISH_BIN="${RISH_PATH:-${HOME}/Rish/rish}"
rsh() {
    [ ! -x "$RISH_BIN" ] && { echo "RISH_MISSING" >&2; return 1; }
    "$RISH_BIN" -c "$*" 2>/dev/null
}

rsh_preflight() {
    rsh "echo PING" 2>/dev/null | grep -q "PING" || {
        printf "${AM}  [WARN] rish unavailable — install Shizuku & pair${RESET}\n"
        return 1
    }
    return 0
}

divider() { printf "${ST}"; printf '%*s' "$W" '' | tr ' ' '-'; printf "${RESET}\n"; }

banner() {
    clear 2>/dev/null || true
    printf "${ST}"; printf '%*s' "$W" '' | tr ' ' '='; printf "${RESET}\n"
    printf "${BOLD}${NG}  RAM SLAMMER v2.2 — Process & Memory Surveillance${RESET}\n"
    printf "${ST}  Companions: ratkingd · leatherheadd · Don${RESET}\n"
    printf "${ST}"; printf '%*s' "$W" '' | tr ' ' '='; printf "${RESET}\n\n"
}

# ── Init DB ───────────────────────────────────────────────────────────────────
init_db() {
    mkdir -p "${BASE}/pipes"
    chmod 700 "${BASE}/pipes"
    trap 'rm -f "${BASE}/pipes/rs_targets_"* "${RAM_STATE}"' EXIT
    sqlite3 "$DB" << 'SQLEOF'
CREATE TABLE IF NOT EXISTS processes (
    pkg TEXT PRIMARY KEY,
    last_action TEXT,
    action_count INTEGER DEFAULT 0,
    respawn_count INTEGER DEFAULT 0,
    last_seen INTEGER,
    flagged_by_don INTEGER DEFAULT 0
);
CREATE TABLE IF NOT EXISTS action_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp INTEGER DEFAULT (strftime('%s','now')),
    pkg TEXT,
    action TEXT,
    reason TEXT
);
SQLEOF
}

# ── Don findings ──────────────────────────────────────────────────────────────
get_don_flagged() {
    if [ ! -f "$LAST_SCAN" ]; then echo ""; return; fi
    python3 -c "
import json, os
path = os.path.expanduser('~/MiuiserPeruser/data/last_scan.json')
try:
    data = json.load(open(path))
    flagged = [f for f in data if f.get('turtle','') in ('DON_BEHAVIOR','DON_INTEGRITY','DON_MEM')]
    for f in flagged:
        print(f.get('description',''))
except:
    pass
" 2>/dev/null
}

# ── Thermal grade ─────────────────────────────────────────────────────────────
get_thermal_grade() {
    if [ -f "$LEATHERHEADD_JSON" ]; then
        python3 -c "
import json
d = json.load(open('$LEATHERHEADD_JSON'))
print(d.get('grade','A'))
" 2>/dev/null || echo "A"
    else
        echo "A"
    fi
}

# ── Tier logic ────────────────────────────────────────────────────────────────
get_tier() {
    local mem_kb="${1:-0}"
    local grade="${2:-A}"
    # Thermal-aware thresholds (in KB)
    local t4=400000 t3=200000 t2=100000
    case "$grade" in
        HOT|C)  t4=200000; t3=150000; t2=80000 ;;
        WARM|B) t4=300000; t3=180000; t2=90000 ;;
    esac
    if [ "$mem_kb" -gt "$t4" ] 2>/dev/null; then echo 4
    elif [ "$mem_kb" -gt "$t3" ] 2>/dev/null; then echo 3
    elif [ "$mem_kb" -gt "$t2" ] 2>/dev/null; then echo 2
    else echo 1
    fi
}

tier_label() {
    case "$1" in
        4) printf "${RD}[CRITICAL]${RESET}" ;;
        3) printf "${AM}[HIGH]${RESET}" ;;
        2) printf "${CY}[MEDIUM]${RESET}" ;;
        *) printf "${NG}[MONITOR]${RESET}" ;;
    esac
}

# ── Log action ──────────────────────────────────────────────────────────────────
log_action() {
    local pkg="$1" action="$2" reason="$3"
    local safe_pkg="${pkg//\'/\'\'}"
    local safe_act="${action//\'/\'\'}"
    local safe_rsn="${reason//\'/\'\'}"
    sqlite3 "$DB" "INSERT OR REPLACE INTO processes(pkg,last_action,action_count,last_seen)
        VALUES('${safe_pkg}','${safe_act}',
        COALESCE((SELECT action_count FROM processes WHERE pkg='${safe_pkg}'),0)+1,
        strftime('%s','now'));"
    sqlite3 "$DB" "INSERT INTO action_log(pkg,action,reason)
        VALUES('${safe_pkg}','${safe_act}','${safe_rsn}');"
}

# ── Apply action ────────────────────────────────────────────────────────────────
apply_action() {
    local pkg="$1" tier="$2" reason="${3:-memory}"
    case "$tier" in
        4)
            printf "${RD}  [SLAM] Force-stopping and disabling: %s${RESET}\n" "$pkg"
            rsh "am force-stop $pkg"
            rsh "pm disable-user --user 0 $pkg"
            log_action "$pkg" "DISABLED" "$reason"
            ;;
        3)
            printf "${AM}  [STOP] Force-stopping: %s${RESET}\n" "$pkg"
            rsh "am force-stop $pkg"
            log_action "$pkg" "FORCE_STOPPED" "$reason"
            ;;
        2)
            printf "${CY}  [RESTRICT] Background restricted: %s${RESET}\n" "$pkg"
            rsh "cmd appops set $pkg RUN_IN_BACKGROUND ignore"
            rsh "cmd appops set $pkg WAKE_LOCK ignore"
            log_action "$pkg" "RESTRICTED" "$reason"
            ;;
        *)
            printf "${NG}  [MONITOR] Watching: %s${RESET}\n" "$pkg"
            log_action "$pkg" "MONITORED" "$reason"
            ;;
    esac
}

# ── Show Don findings ───────────────────────────────────────────────────────────
show_don_findings() {
    banner
    printf "${BOLD}${NG}  [DON] Behavioral Findings${RESET}\n"
    divider
    local don
    don=$(get_don_flagged)
    if [ -z "$don" ]; then
        printf "${NG}  [OK] No Don findings in last scan${RESET}\n\n"
    else
        echo "$don" | while read -r line; do
            [ -n "$line" ] && printf "${AM}  [!!] %s${RESET}\n" "$line"
        done
        printf "\n"
    fi

    if [ -f "$LEATHERHEADD_JSON" ]; then
        printf "${BOLD}${NG}  [LEATHERHEADD] Thermal Context${RESET}\n"
        divider
        python3 -c "
import json, os
path = os.path.expanduser('~/MiuiserPeruser/Registry/daemon_results/leatherheadd.json')
try:
    d = json.load(open(path))
    print(f'  Grade:    {d.get(\"grade\",\"?\")}')
    print(f'  Thermal:  {d.get(\"thermal_score\",\"?\")}')
    print(f'  Throttled: {d.get(\"throttled_cores\",\"?\")}/{d.get(\"total_cores\",\"?\")}')
except Exception as e:
    print(f'  Error: {e}')
" 2>/dev/null
        printf "\n"
    fi
}

# ── Live process scan (KILL BOARD) ────────────────────────────────────────────
show_live_scan() {
    mkdir -p "${BASE}/pipes"

    while true; do
        banner
        printf "${BOLD}${NG}  [KILL BOARD] Top RAM Hogs${RESET}\n"
        divider

        local thermal_grade
        thermal_grade=$(get_thermal_grade)
        printf "${ST}  Thermal: ${AM}%s${RESET}  ${ST}%s${RESET}\n\n" "$thermal_grade" "$(date '+%H:%M:%S')"

        # Load previous state for delta tracking
        declare -A PREV_MEM=()
        if [ -f "$RAM_STATE" ]; then
            while IFS=' ' read -r spkg smem; do
                [ -n "$spkg" ] && PREV_MEM["$spkg"]="$smem"
            done < "$RAM_STATE"
        fi
: > "$RAM_STATE"

        # Get memory data — try multiple parsers
        local memdata=""

        # Parser 1: dumpsys meminfo "Total RSS by process" (HyperOS 2.0 format)
        memdata=$(rsh "dumpsys meminfo 2>/dev/null | grep -A 30 'Total RSS by process' | grep 'K:' | sed 's/,//g' | sed 's/K://g' | awk '{print \$1, \$2}' | sort -rn | head -12" 2>/dev/null)

        # Parser 2: dumpsys meminfo generic format
        if [ -z "$memdata" ]; then
            memdata=$(rsh "dumpsys meminfo 2>/dev/null | grep -E '^\s+[0-9,]+K:' | sed 's/,//g; s/K://g' | awk '{print \$1, \$2}' | sort -rn | head -12" 2>/dev/null)
        fi

        # Parser 3: /proc VmRSS fallback
        if [ -z "$memdata" ]; then
            memdata=$(rsh "for p in /proc/[0-9]*; do [ -f \$p/status ] || continue; rss=\$(grep VmRSS \$p/status 2>/dev/null | awk '{print \$2}'); [ -z \"\$rss\" ] && continue; cmd=\$(cat \$p/cmdline 2>/dev/null | tr '\\0' ' ' | cut -d' ' -f1); [ -z \"\$cmd\" ] && continue; echo \"\$rss \$cmd\"; done 2>/dev/null | sort -rn | head -12" 2>/dev/null)
        fi

        if [ -z "$memdata" ]; then
            printf "${AM}  [WARN] No memory data — rish may be disconnected${RESET}\n\n"
            printf "${NG}  [r]efresh  [b]ack: ${RESET}"
            read -r choice
            [ "$choice" = "b" ] || [ "$choice" = "B" ] && return
            continue
        fi

        declare -a TARGETS=()
        local idx=1

        while IFS=' ' read -r mem_kb pkg; do
            [ -z "$pkg" ] || [ -z "$mem_kb" ] && continue
            [ "$mem_kb" -eq 0 ] 2>/dev/null && continue

            # Skip unkillable system processes
            case "$pkg" in
                init|zygote|zygote64|surfaceflinger|vold|logd|installd|\
                com.android.systemui|com.android.phone|com.miui.home|\
                com.termux|com.termux.api|com.termux.boot|\
                moe.shizuku.privileged.api|\
                com.google.android.gms|com.google.android.gsf|\
                android) continue ;;
            esac

            local mem_mb=$(( mem_kb / 1024 ))
            local tier
            tier=$(get_tier "$mem_kb" "$thermal_grade")

            # Delta tracking
            local prev="${PREV_MEM[$pkg]:-0}"
            local arrow="~" note=""
            if [ "$prev" -gt 0 ] 2>/dev/null; then
                local delta=$(( (mem_kb - prev) * 100 / prev ))
                if [ "$delta" -gt 300 ] 2>/dev/null; then
                    arrow="${RD}▲+${delta}%${RESET}"
                    note="${RD}<-SPIKE${RESET}"
                elif [ "$delta" -gt 20 ] 2>/dev/null; then
                    arrow="${AM}▲+${delta}%${RESET}"
                elif [ "$delta" -lt -20 ] 2>/dev/null; then
                    arrow="${NG}▼${delta}%${RESET}"
                fi
            fi

            printf "  ${ST}[%2d]${RESET} ${WH}%-42s${RESET} ${CY}%4dMB${RESET}  " "$idx" "$pkg" "$mem_mb"
            tier_label "$tier"
            printf "  %b %b\n" "$arrow" "$note"

            # MSA kill spike annotation
            if [ -n "$note" ]; then
                local recent_msa
                recent_msa=$(sqlite3 "$DB" "SELECT COUNT(*) FROM action_log WHERE pkg='com.miui.msa.global' AND action='FORCE_STOPPED' AND timestamp > strftime('%s','now') - 60;" 2>/dev/null)
                [ "${recent_msa:-0}" -gt 0 ] 2>/dev/null && \
                    printf "    ${RD}  <- MSA KILL SPIKE — buffer reallocated${RESET}\n"
            fi

            echo "$pkg $mem_kb" >> "$RAM_STATE"
            TARGETS[idx]="$pkg"
            idx=$((idx+1))
        done <<< "$memdata"

        printf "\n"
        divider
        printf "${NG}  Number to target, [r]efresh, [b]ack: ${RESET}"
        read -r choice

        case "$choice" in
            b|B) return ;;
            r|R) continue ;;
            [0-9]*)
                if [ "$choice" -ge 1 ] && [ "$choice" -lt "$idx" ] 2>/dev/null; then
                    local tpkg="${TARGETS[$choice]}"
                    if [ -n "$tpkg" ]; then
                        printf "\n${AM}  Target: ${WH}%s${RESET}\n\n" "$tpkg"
                        printf "${NG}  [1]${RESET} Force-stop  ${NG}[2]${RESET} Restrict  ${NG}[3]${RESET} Disable  ${NG}[4]${RESET} Investigate  ${NG}[b]${RESET} Cancel\n"
                        printf "${NG}  Action: ${RESET}"
                        read -r act
                        case "$act" in
                            1)
                                rsh "am force-stop $tpkg"
                                log_action "$tpkg" "FORCE_STOPPED" "kill_board"
                                printf "${NG}  Stopped. Refresh to see spike effects.${RESET}\n" ;;
                            2)
                                rsh "cmd appops set $tpkg RUN_IN_BACKGROUND ignore"
                                log_action "$tpkg" "RESTRICTED" "kill_board"
                                printf "${NG}  Background restricted.${RESET}\n" ;;
                            3)
                                rsh "pm disable-user --user 0 $tpkg"
                                log_action "$tpkg" "DISABLED" "kill_board"
                                printf "${NG}  Disabled.${RESET}\n" ;;
                            4)
                                printf "\n${CY}  Recent actions for %s:${RESET}\n" "$tpkg"
                                sqlite3 "$DB" "SELECT datetime(timestamp,'unixepoch'), action, reason FROM action_log WHERE pkg='${tpkg//\'/\'\'}' ORDER BY timestamp DESC LIMIT 5;" 2>/dev/null | \
                                    while IFS='|' read -r ts a r; do
                                        printf "  ${ST}%s${RESET}  ${AM}%s${RESET}  %s\n" "$ts" "$a" "$r"
                                    done ;;
                            b|B) : ;;
                        esac
                        printf "\n${NG}  Press enter...${RESET}"; read -r
                    fi
                else
                    printf "${AM}  Invalid selection${RESET}\n"
                    printf "${NG}  Press enter...${RESET}"; read -r
                fi
                ;;
            *)
                printf "${AM}  Invalid input${RESET}\n"
                printf "${NG}  Press enter...${RESET}"; read -r
                ;;
        esac
    done
}

# ── Action menu ───────────────────────────────────────────────────────────────
action_menu() {
    banner
    printf "${BOLD}${NG}  [ACTIONS] Process Control${RESET}\n"
    divider
    printf "\n"
    printf "${NG}  [1]${RESET} ${WH}Kill all Xiaomi telemetry${RESET}\n"
    printf "${NG}  [2]${RESET} ${WH}Restrict MIUI Analytics${RESET}\n"
    printf "${NG}  [3]${RESET} ${WH}Force-stop MSA${RESET}\n"
    printf "${NG}  [4]${RESET} ${WH}Kill MiuiDaemon${RESET}\n"
    printf "${NG}  [5]${RESET} ${WH}Show action history${RESET}\n"
    printf "${NG}  [b]${RESET} ${WH}Back${RESET}\n"
    printf "\n"
    divider
    printf "${NG}  Choice: ${RESET}"
    read -r choice

    case "$choice" in
        1)
            for pkg in com.miui.msa.global com.miui.analytics com.miui.daemon; do
                printf "${AM}  Killing %s...${RESET}\n" "$pkg"
                rsh "am force-stop $pkg"
                log_action "$pkg" "FORCE_STOPPED" "telemetry_kill"
            done ;;
        2)
            rsh "cmd appops set com.miui.analytics RUN_IN_BACKGROUND ignore"
            log_action "com.miui.analytics" "RESTRICTED" "manual" ;;
        3)
            rsh "am force-stop com.miui.msa.global"
            log_action "com.miui.msa.global" "FORCE_STOPPED" "manual" ;;
        4)
            rsh "am force-stop com.miui.daemon"
            log_action "com.miui.daemon" "FORCE_STOPPED" "manual" ;;
        5)
            printf "\n${NG}  Recent actions:${RESET}\n"
            sqlite3 "$DB" "SELECT datetime(timestamp,'unixepoch'), pkg, action, reason FROM action_log ORDER BY timestamp DESC LIMIT 20;" 2>/dev/null | \
                while IFS='|' read -r ts pkg action reason; do
                    printf "  ${ST}%s${RESET}  %-40s ${AM}%s${RESET}  %s\n" "$ts" "$pkg" "$action" "$reason"
                done ;;
        b|B) return ;;
    esac
    printf "\n${NG}  Press enter...${RESET}"; read -r
}

# ── Memory pressure summary ───────────────────────────────────────────────────
show_memory_summary() {
    banner
    printf "${BOLD}${NG}  [RAM] Memory Pressure Summary${RESET}\n"
    divider

    local total avail
    total=$(rsh "grep MemTotal /proc/meminfo 2>/dev/null | awk '{print \$2}'" | tr -d '\n')
    avail=$(rsh "grep MemAvailable /proc/meminfo 2>/dev/null | awk '{print \$2}'" | tr -d '\n')
    total="${total:-0}"
    avail="${avail:-0}"
    local total_mb=$(( total / 1024 ))
    local avail_mb=$(( avail / 1024 ))
    local used_mb=$(( total_mb - avail_mb ))
    local pct=$(( avail_mb * 100 / (total_mb + 1) ))

    printf "  ${WH}Total RAM:${RESET}     ${CY}%d MB${RESET}\n" "$total_mb"
    printf "  ${WH}Used:${RESET}          ${AM}%d MB${RESET}\n" "$used_mb"
    printf "  ${WH}Available:${RESET}     ${NG}%d MB${RESET} (%d%%)\n" "$avail_mb" "$pct"

    if [ "$pct" -lt 15 ] 2>/dev/null; then
        printf "\n  ${RD}[CRITICAL] Memory pressure — <15%% available${RESET}\n"
    elif [ "$pct" -lt 30 ] 2>/dev/null; then
        printf "\n  ${AM}[HIGH] Memory pressure — <30%% available${RESET}\n"
    else
        printf "\n  ${NG}[OK] Memory pressure normal${RESET}\n"
    fi
    printf "\n"
}

# ── Main ──────────────────────────────────────────────────────────────────────
init_db
rsh_preflight || true

while true; do
    banner
    printf "${BOLD}${NG}  Main Menu${RESET}\n"
    divider
    printf "\n"
    printf "${NG}  [1]${RESET} ${WH}Don behavioral findings${RESET}\n"
    printf "${NG}  [2]${RESET} ${WH}Live process scan (Kill Board)${RESET}\n"
    printf "${NG}  [3]${RESET} ${WH}Memory pressure summary${RESET}\n"
    printf "${NG}  [4]${RESET} ${WH}Process actions${RESET}\n"
    printf "${NG}  [q]${RESET} ${WH}Quit${RESET}\n"
    printf "\n"
    divider
    printf "${NG}  Choice: ${RESET}"
    read -r choice

    case "$choice" in
        1) show_don_findings; printf "${NG}  Press enter...${RESET}"; read -r ;;
        2) show_live_scan ;;
        3) show_memory_summary; printf "${NG}  Press enter...${RESET}"; read -r ;;
        4) action_menu ;;
        q|Q) printf "\n${NG}  Ram Slammer out.${RESET}\n\n"; exit 0 ;;
        *) continue ;;
    esac
done
