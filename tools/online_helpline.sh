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
DB="$BASE/data/miuiserperuser.db"
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
    printf "${YL}${BOLD}  [LOG] Stored Network Findings${RESET}
"
    hline

    SCAN_JSON="$HOME/MiuiserPeruser/data/last_scan.json"
    if [ ! -f "$SCAN_JSON" ]; then
        printf "${OR}  No scan results found. Run superhero --standard first.${RESET}

"
        return
    fi

    printf "${TL}  Last scan results:${RESET}

"
    python3 << 'PYEOF2'
import json
path = __import__('os').path.expanduser('~/MiuiserPeruser/data/last_scan.json')
data = json.load(open(path))
for f in data:
    pri = f.get('priority','LOW')
    typ = f.get('type','?')
    desc = f.get('description','')
    turtle = f.get('turtle','?')
    icons = {'CRITICAL':'[!!]','HIGH':'[HI]','MEDIUM':'[MD]','LOW':'[LO]'}
    icon = icons.get(pri,'[??]')
    print(f'  {icon} [{pri}][{turtle}] {typ}')
    print(f'     {desc}')
    print()
PYEOF2
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


# ── Explanations ──────────────────────────────────────────────────────────────
show_explanations() {
    banner
    printf "${YL}${BOLD}  [?] What Does It All Mean?${RESET}\n"
    hline
    echo

    printf "${YL}  SNO_TRACKING${RESET}\n"
    printf "${WH}  Xiaomi bakes a unique Serial Number Order ID into every device at the factory.\n"
    printf "${WH}  It survives factory resets and is used to track you across devices and ad networks.\n"
    printf "${TL}  Action: Cannot be removed without reflashing the ROM.${RESET}\n\n"

    printf "${YL}  FB_PARTNER_ID${RESET}\n"
    printf "${WH}  A Facebook partnership token pre-installed by Xiaomi. Any app using the\n"
    printf "${WH}  Facebook SDK will automatically attribute installs to Xiaomi without your consent.\n"
    printf "${TL}  Action: Disable Facebook app and block com.facebook.* via netpolicy.${RESET}\n\n"

    printf "${YL}  MIUI_ANALYTICS${RESET}\n"
    printf "${WH}  Confirmed by TCD Dublin academic research (2021) to send encrypted telemetry\n"
    printf "${WH}  including screen touches and app interaction logs to data.mistat.intl.xiaomi.com.\n"
    printf "${TL}  Action: Online Helpline > Actions > Disable MIUI Analytics.${RESET}\n\n"

    printf "${YL}  MIUI_DAEMON${RESET}\n"
    printf "${WH}  Background data collection service. Runs continuously, collects device\n"
    printf "${WH}  usage patterns and hardware telemetry.\n"
    printf "${TL}  Action: Online Helpline > Actions > Disable MiuiDaemon.${RESET}\n\n"

    printf "${YL}  MILLET_ACTIVE${RESET}\n"
    printf "${WH}  Xiaomi kernel-level process scheduler. Monitors binder IPC calls between\n"
    printf "${WH}  all apps. Cannot be disabled without losing system stability.\n"
    printf "${TL}  Action: Informational only — baked into HyperOS kernel.${RESET}\n\n"

    printf "${YL}  GDPR_OPT_OUT${RESET}\n"
    printf "${WH}  Your device is in Ireland (EEA). Xiaomi is legally required to obtain\n"
    printf "${WH}  explicit consent before collecting telemetry. The opt-out prop is not set,\n"
    printf "${WH}  meaning consent may not have been properly recorded.\n"
    printf "${TL}  Action: Settings > Privacy > opt out of all Xiaomi data programmes.${RESET}\n\n"

    printf "${YL}  ADB_TCP_ACTIVE${RESET}\n"
    printf "${WH}  ADB over TCP is enabled on port 5555. This is how MiuiserPeruser talks\n"
    printf "${WH}  to your device. Only a risk if exposed beyond loopback (127.0.0.1).\n"
    printf "${TL}  Action: Disable when not in use: adb disconnect.${RESET}\n\n"

    printf "${YL}  PARTNER_TOKEN (AppsFlyer/Netflix/Google)${RESET}\n"
    printf "${WH}  Pre-installed attribution tokens that tell third parties this device\n"
    printf "${WH}  is a Xiaomi unit. Used for ad attribution and partnership tracking.\n"
    printf "${TL}  Action: Cannot be removed without reflashing. Passive — no active transmission.${RESET}\n\n"

    hline
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
    printf "${TL}  [4]${RESET} ${WH}What does it all mean?${RESET}\n"
    printf "${TL}  [q]${RESET} ${WH}Quit${RESET}\n"
    echo
    hline
    printf "${YL}  Choice: ${RESET}"
    read -r choice

    case "$choice" in
        1) show_stored_findings; printf "${YL}  Press enter...${RESET}"; read -r ;;
        2) run_fresh_scan;       printf "${YL}  Press enter...${RESET}"; read -r ;;
        3) action_menu ;;
        4) show_explanations; printf "${YL}  Press enter...${RESET}"; read -r ;;
        q|Q) break ;;
    esac
done

printf "\n${TL}  [TEL]  Online Helpline closed.${RESET}\n\n"
