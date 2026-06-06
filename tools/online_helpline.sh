#!/data/data/com.termux/files/usr/bin/bash
source "$(dirname "$0")/lib/tool_backend.sh"
# ==============================================================================
#  Online Helpline — Network Anomaly Response Tool
#  Companion to: rahzerd daemon + RAPH scanner
#  Reads: Registry/daemon_results/rahzerd.json + data/sensei_dojo.db
#  Theme: Childline yellow & teal — urgent, warm, action-oriented
# ==============================================================================

BASE="$HOME/MiuiserPeruser"
RAHZERD_RESULTS="$BASE/Registry/daemon_results/rahzerd.json"
DB="$BASE/data/sensei_dojo.db"
RAHZERD_BIN="$BASE/bin/rahzerd"

# ── Childline palette ─────────────────────────────────────────────────────────
YL='\033[38;5;220m'   # childline yellow
TL='\033[38;5;37m'    # childline teal
DT='\033[38;5;30m'    # dark teal
LY='\033[38;5;229m'   # light yellow
WH='\033[38;5;255m'   # white
RD='\033[38;5;196m'   # alert red
OR='\033[38;5;208m'   # orange warning
GR='\033[38;5;82m'    # green clear
DK='\033[38;5;235m'   # dark background
BOLD='\033[1m'
RESET='\033[0m'

COLS=$(tput cols 2>/dev/null || echo 60)
W=$(( COLS - 4 )); [ "$W" -gt 72 ] && W=72

# ── Drawing helpers ───────────────────────────────────────────────────────────
hline() { printf "${TL}"; printf '%*s' "$W" '' | tr ' ' '-'; printf "${RESET}\n"; }
dline() { printf "${YL}"; printf '%*s' "$W" '' | tr ' ' '='; printf "${RESET}\n"; }

banner() {
    clear
    dline
    printf "${YL}${BOLD}  [TEL]  ONLINE HELPLINE — Network Anomaly Response${RESET}\n"
    printf "${TL}     Companion: rahzerd · RAPH · 15-layer connectivity audit${RESET}\n"
    dline
    echo
}

# ── Privileged shell ──────────────────────────────────────────────────────────
rsh() { _backend_exec "$@" 2>/dev/null; }

# ── Show stored RAPH findings from sensei_dojo.db ─────────────────────────────
show_stored_findings() {
    banner
    printf "${YL}${BOLD}  [LOG] Stored Network Findings${RESET}\n"
    hline

    if [ ! -f "$DB" ]; then
        printf "${OR}  No scan database found. Run a fresh scan first.${RESET}\n\n"
        return
    fi

    local findings
    findings=$(sqlite3 "$DB" \
        "SELECT priority, detection_type, description FROM detections
         WHERE detection_class='NETWORK' OR detection_class='BEHAVIOR'
         ORDER BY priority DESC LIMIT 50;" 2>/dev/null)

    if [ -z "$findings" ]; then
        printf "${GR}  [OK]  No network findings stored.${RESET}\n\n"
    else
        echo "$findings" | while IFS='|' read -r pri type desc; do
            case "$pri" in
                CRITICAL) col="${RD}" icon="[!!]" ;;
                HIGH)     col="${OR}" icon="[HI]" ;;
                MEDIUM)   col="${YL}" icon="[MD]" ;;
                *)        col="${TL}" icon="[LO]" ;;
            esac
            printf "${col}  ${icon} [${pri}] ${type}${RESET}\n"
            printf "${WH}     ${desc}${RESET}\n\n"
        done
    fi
    hline

    # Also show rahzerd results if available
    if [ -f "$RAHZERD_RESULTS" ]; then
        printf "\n${TL}${BOLD}  [NET] rahzerd Last Report${RESET}\n"
        hline
        python3 -c "
import json, sys
try:
    d = json.load(open('$RAHZERD_RESULTS'))
    for k, v in d.items():
        if isinstance(v, dict):
            status = v.get('status', v.get('result', 'unknown'))
            flag = '[OK]' if 'ok' in str(status).lower() or 'pass' in str(status).lower() else '[!!]'
            print(f'  {flag}  {k}: {status}')
        else:
            print(f'  →  {k}: {v}')
except Exception as e:
    print(f'  Could not parse rahzerd results: {e}')
" 2>/dev/null
    fi
}

# ── Run fresh scan ────────────────────────────────────────────────────────────
run_fresh_scan() {
    banner
    printf "${YL}${BOLD}  [SCN] Running Fresh Network Scan${RESET}\n"
    hline
    echo

    printf "${TL}  [1/3] Launching rahzerd scan...${RESET}\n"
    if [ -x "$RAHZERD_BIN" ]; then
        RISH_APPLICATION_ID=com.termux timeout 30 "$RAHZERD_BIN" --once 2>/dev/null &
        local rpid=$!
        printf "${TL}        rahzerd running (pid $rpid)...${RESET}\n"
        wait $rpid 2>/dev/null
        printf "${GR}        [OK] rahzerd complete${RESET}\n"
    else
        printf "${OR}        rahzerd binary not found at $RAHZERD_BIN${RESET}\n"
    fi
    echo

    printf "${TL}  [2/3] Checking active connections...${RESET}\n"
    local conns
    conns=$(rsh "ss -tnp 2>/dev/null | grep ESTAB | grep -v '127\.' | head -10")
    if [ -n "$conns" ]; then
        printf "${OR}  Active external connections:${RESET}\n"
        echo "$conns" | while read -r line; do
            printf "${WH}    $line${RESET}\n"
        done
    else
        printf "${GR}        [OK] No external connections${RESET}\n"
    fi
    echo

    printf "${TL}  [3/3] Checking for Xiaomi telemetry...${RESET}\n"
    local mistat
    mistat=$(rsh "ss -tnp 2>/dev/null | grep -iE 'mistat|xiaomi|miui' | head -5")
    if [ -n "$mistat" ]; then
        printf "${RD}  [!!]  Xiaomi telemetry connections active:${RESET}\n"
        echo "$mistat" | while read -r line; do
            printf "${OR}    $line${RESET}\n"
        done
    else
        printf "${GR}        [OK] No Xiaomi telemetry connections detected${RESET}\n"
    fi
    echo
    hline
    printf "${GR}  Scan complete.${RESET}\n\n"
}

# ── Action menu ───────────────────────────────────────────────────────────────
action_menu() {
    banner
    printf "${YL}${BOLD}  [ACT] Network Actions${RESET}\n"
    hline
    echo
    printf "${TL}  [1]${RESET} ${WH}Disable MSA (MIUI System Ads)${RESET}\n"
    printf "${TL}  [2]${RESET} ${WH}Disable MIUI Analytics${RESET}\n"
    printf "${TL}  [3]${RESET} ${WH}Disable MiuiDaemon${RESET}\n"
    printf "${TL}  [4]${RESET} ${WH}Disable Xiaomi Discover / GetApps${RESET}\n"
    printf "${TL}  [5]${RESET} ${WH}Block background network for MSA${RESET}\n"
    printf "${TL}  [6]${RESET} ${WH}Kill active Xiaomi telemetry PIDs${RESET}\n"
    printf "${TL}  [7]${RESET} ${WH}Show active connections with PIDs${RESET}\n"
    printf "${TL}  [b]${RESET} ${WH}Back${RESET}\n"
    echo
    hline
    printf "${YL}  Choice: ${RESET}"
    read -r choice

    case "$choice" in
        1)
            printf "${TL}  Disabling com.miui.msa.global...${RESET}\n"
            rsh "pm disable-user --user 0 com.miui.msa.global"
            printf "${GR}  [OK] Done${RESET}\n" ;;
        2)
            printf "${TL}  Disabling com.miui.analytics...${RESET}\n"
            rsh "pm disable-user --user 0 com.miui.analytics"
            printf "${GR}  [OK] Done${RESET}\n" ;;
        3)
            printf "${TL}  Disabling com.miui.daemon...${RESET}\n"
            rsh "pm disable-user --user 0 com.miui.daemon"
            printf "${GR}  [OK] Done${RESET}\n" ;;
        4)
            printf "${TL}  Disabling com.xiaomi.discover...${RESET}\n"
            rsh "pm disable-user --user 0 com.xiaomi.discover"
            printf "${GR}  [OK] Done${RESET}\n" ;;
        5)
            local uid
            uid=$(rsh "pm list packages -U 2>/dev/null | grep com.miui.msa.global | grep -oE 'uid:[0-9]+' | cut -d: -f2")
            if [ -n "$uid" ]; then
                rsh "cmd netpolicy deny-metered-background $uid"
                printf "${GR}  [OK] Background network denied for MSA (uid $uid)${RESET}\n"
            else
                printf "${OR}  Could not find MSA uid${RESET}\n"
            fi ;;
        6)
            printf "${TL}  Killing Xiaomi telemetry processes...${RESET}\n"
            local pids
            pids=$(rsh "ps -A 2>/dev/null | grep -iE 'msa|miui.analytics|miui.daemon' | awk '{print \$2}'")
            if [ -n "$pids" ]; then
                echo "$pids" | while read -r pid; do
                    rsh "kill $pid"
                    printf "${GR}  [OK] Killed PID $pid${RESET}\n"
                done
            else
                printf "${GR}  No telemetry processes found${RESET}\n"
            fi ;;
        7)
            printf "${TL}  Active connections:${RESET}\n"
            rsh "ss -tnp 2>/dev/null | grep ESTAB" | while read -r line; do
                printf "${WH}  $line${RESET}\n"
            done ;;
        b|B) return ;;
    esac
    echo
    printf "${YL}  Press enter to continue...${RESET}"
    read -r
}

# ── Main loop ─────────────────────────────────────────────────────────────────
while true; do
    banner
    printf "${YL}${BOLD}  Main Menu${RESET}\n"
    hline
    echo
    printf "${TL}  [1]${RESET} ${WH}View stored findings${RESET}\n"
    printf "${TL}  [2]${RESET} ${WH}Run fresh scan${RESET}\n"
    printf "${TL}  [3]${RESET} ${WH}Network actions (block/kill)${RESET}\n"
    printf "${TL}  [q]${RESET} ${WH}Quit${RESET}\n"
    echo
    hline
    printf "${YL}  Choice: ${RESET}"
    read -r choice

    case "$choice" in
        1) show_stored_findings; printf "${YL}  Press enter...${RESET}"; read -r ;;
        2) run_fresh_scan;       printf "${YL}  Press enter...${RESET}"; read -r ;;
        3) action_menu ;;
        q|Q) break ;;
    esac
done

printf "\n${TL}  [TEL]  Online Helpline closed.${RESET}\n\n"
