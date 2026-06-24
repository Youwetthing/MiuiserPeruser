#!/data/data/com.termux/files/usr/bin/bash
# ==============================================================================
#  Channel 6 Newsroom — Operator Briefing & Tool Recommendation Engine
#  Reads all daemon result JSONs and surfaces actionable intelligence
#  Theme: Pop art tabloid — black/white newsprint, bold colour headline blocks
# ==============================================================================

BASE="$HOME/MiuiserPeruser"
RESULTS="$BASE/Registry/daemon_results"
TOOLS="$BASE/tools"

# ── Newsprint palette ─────────────────────────────────────────────────────────
BLACK='\033[38;5;16m'      # pure black
WHITE='\033[38;5;255m'     # pure white
BGBLACK='\033[48;5;16m'    # black background
BGWHITE='\033[48;5;255m'   # white background
BGRED='\033[48;5;196m'     # red block — URGENT
BGYEL='\033[48;5;226m'     # yellow block — WARNING
BGCYAN='\033[48;5;51m'     # cyan block — NOTICE
BGBLUE='\033[48;5;21m'     # blue block — headers
INK='\033[38;5;236m'       # dark ink — body
GREY='\033[38;5;244m'      # mid grey — secondary
LTGREY='\033[38;5;251m'    # light grey — caption
RED='\033[38;5;196m'       # red text
YEL='\033[38;5;226m'       # yellow text
CYAN='\033[38;5;51m'       # cyan text
BLUE='\033[38;5;21m'       # blue text
BOLD='\033[1m'
RESET='\033[0m'

COLS=$(tput cols 2>/dev/null || echo 64)
W=$(( COLS - 2 ))
DATE=$(date "+%A %B %d, %Y")
TIME=$(date "+%H:%M")

# ── Helpers ───────────────────────────────────────────────────────────────────
rule()  { echo -e "${INK}$(printf '%*s' "$COLS" | tr ' ' '-')${RESET}"; }
drule() { echo -e "${BOLD}${BLACK}$(printf '%*s' "$COLS" | tr ' ' '=')${RESET}"; }
hrule() { echo -e "${BOLD}${BLACK}$(printf '%*s' "$COLS" | tr ' ' '#')${RESET}"; }

# Print a colour block headline
block_head() {
    local bg=$1 fg=$2 text=$3
    local inner=$(( W - 2 ))
    local pad=$(( (inner - ${#text}) / 2 )); [ $pad -lt 0 ] && pad=0
    local rpad=$(( inner - pad - ${#text} )); [ $rpad -lt 0 ] && rpad=0
    echo -e "${bg}${fg}${BOLD}$(printf '%*s' "$((W))" | tr ' ' ' ')${RESET}"
    printf "${bg}${fg}${BOLD} %${pad}s%s%${rpad}s ${RESET}\n" "" "$text" ""
    echo -e "${bg}${fg}${BOLD}$(printf '%*s' "$((W))" | tr ' ' ' ')${RESET}"
}

# ── Masthead ──────────────────────────────────────────────────────────────────
masthead() {
    clear
    hrule
    block_head "$BGBLUE" "$WHITE" "CHANNEL 6 NEWSROOM"
    hrule
    printf " ${BOLD}${BLACK}%-$((W/2))s${RESET}${GREY}%s${RESET}\n" \
           "\"Keeping New York Informed\"" "${DATE}  ${TIME}"
    printf " ${GREY}Field Reporter: April O'Neil  |  MiuiserPeruser Intelligence Bureau${RESET}\n"
    hrule
    echo ""
}

# ── Finding storage ────────────────────────────────────────────────────────────
FINDINGS=()

add_finding() {
    local severity=$1 daemon=$2 headline=$3 detail=$4 tool=$5 action=$6
    FINDINGS+=("${severity}|||${daemon}|||${headline}|||${detail}|||${tool}|||${action}")
}

# ── Parsers ───────────────────────────────────────────────────────────────────
parse_burned() {
    local f="$RESULTS/burned.json"; [ ! -f "$f" ] && return
    local privacy_list score
    privacy_list=$(jq -r '.privacy_list // ""' "$f" 2>/dev/null)
    score=$(jq -r '.privacy_signal_count // 0' "$f" 2>/dev/null)

    echo "$privacy_list" | tr ',' '\n' | while read -r sig; do
        case "$sig" in
            FACEBOOK_PARTNER_BAKED)
                add_finding "WARNING" "burned" \
                    "FACEBOOK PARTNER ID BAKED INTO ROM" \
                    "Xiaomi has embedded a Facebook tracking ID in system props. Cannot be removed without ROM flash." \
                    "StalkerSlayer.sh" "Block telemetry endpoints" ;;
            APPSFLYER_PREINSTALL)
                add_finding "WARNING" "burned" \
                    "APPSFLYER TRACKING PREINSTALLED" \
                    "Mobile attribution tracker at /data/miui/pai/ — reporting installs to AppsFlyer." \
                    "StalkerSlayer.sh" "Disable AppsFlyer reporting" ;;
            FORCED_REBOOT_POLICY)
                add_finding "WARNING" "burned" \
                    "FORCED REBOOT EVERY 7 DAYS" \
                    "Device scheduled to force reboot on a 7-day cycle by MIUI policy." \
                    "hidden_settings.sh" "Disable reboot policy" ;;
            HYPEROS_DETECTED)
                add_finding "NOTICE" "burned" \
                    "HYPEROS POLICY LAYER ACTIVE" \
                    "MIUI/HyperOS enforcement policies detected in system properties." \
                    "hidden_settings.sh" "Review MIUI hidden settings" ;;
        esac
    done

    [ "${score:-0}" -ge 5 ] && add_finding "WARNING" "burned" \
        "${score} PRIVACY SIGNALS DETECTED" \
        "High volume of tracking and privacy signals in system properties." \
        "StalkerSlayer.sh" "Run full telemetry sweep"
}

parse_metalheadd() {
    local f="$RESULTS/metalheadd.json"; [ ! -f "$f" ] && return
    local sensitive score grade
    sensitive=$(jq -r '.sensitive_active // 0' "$f" 2>/dev/null)
    score=$(jq -r '.sensor_score // 100' "$f" 2>/dev/null)
    grade=$(jq -r '.grade // "CLEAN"' "$f" 2>/dev/null)

    [ "${sensitive:-0}" -gt 2 ] && add_finding "WARNING" "metalheadd" \
        "${sensitive} SENSITIVE SENSORS ACTIVE" \
        "Accelerometer/gyro/magnetometer being polled. Possible location fingerprinting by background apps." \
        "multi_sensory_room.sh" "Investigate + revoke sensor access (option 5)"

    [ "${score:-100}" -lt 70 ] && add_finding "URGENT" "metalheadd" \
        "SENSOR SCORE ${score}/100 — ${grade}" \
        "Abnormal sensor activity detected. Immediate investigation recommended." \
        "multi_sensory_room.sh" "Watch mode + investigate connections"
}

parse_shredderd() {
    local f="$RESULTS/shredderd.json"; [ ! -f "$f" ] && return
    local score grade drift confirmed selinux
    score=$(jq -r '.integrity.score // .integrity_score // 100' "$f" 2>/dev/null)
    grade=$(jq -r '.integrity.grade // .grade // "CLEAN"' "$f" 2>/dev/null)
    drift=$(jq -r '.drift.confirmed // false' "$f" 2>/dev/null)
    selinux=$(jq -r '.integrity.selinux // "unknown"' "$f" 2>/dev/null)

    [ "${score:-100}" -lt 80 ] && add_finding "WARNING" "shredderd"         "KERNEL INTEGRITY ${score}/100 — ${grade}"         "Kernel integrity degraded. SELinux: ${selinux}. Review kernel parameters."         "kernel_sanders.sh" "Kernel audit (option 2)"

    [ "$drift" = "true" ] && add_finding "URGENT" "shredderd"         "CONFIRMED KERNEL MODULE DRIFT"         "Module count changed since baseline — possible rootkit insertion."         "kernel_sanders.sh" "Module audit (option 4)"
}

parse_rahzerd() {
    local f="$RESULTS/rahzerd.json"; [ ! -f "$f" ] && return
    local wifi mobile dns tcp4 tcp6 rat roaming div

    wifi=$(jq -r '.wifi.connected // -1' "$f" 2>/dev/null)
    mobile=$(jq -r '.mobile.data_active // -1' "$f" 2>/dev/null)
    dns=$(jq -r '.dns.resolves // -1' "$f" 2>/dev/null)
    tcp4=$(jq -r '.ports.established_tcp4 // 0' "$f" 2>/dev/null)
    tcp6=$(jq -r '.ports.established_tcp6 // 0' "$f" 2>/dev/null)
    rat=$(jq -r '.mobile.rat // "UNKNOWN"' "$f" 2>/dev/null)
    roaming=$(jq -r '.mobile.roaming // 0' "$f" 2>/dev/null)
    div=$(jq -r '.xiaomi_divergence // 0' "$f" 2>/dev/null)

    [ "$dns" = "0" ] && add_finding "URGENT" "rahzerd"         "DNS RESOLUTION FAILURE"         "Device cannot resolve DNS — possible network hijack or captive portal."         "april_oneil.sh" "Refresh Channel 6"

    [ "$roaming" = "1" ] && add_finding "WARNING" "rahzerd"         "DEVICE ROAMING — ${rat}"         "Mobile data active on roaming network. Higher interception risk."         "april_oneil.sh" "Check connectivity"

    [ "${tcp4:-0}" -gt 50 ] && add_finding "WARNING" "rahzerd"         "HIGH CONNECTION COUNT — ${tcp4} TCP4 + ${tcp6} TCP6"         "Unusually high number of active connections. Possible data exfiltration."         "april_oneil.sh" "Check ports"

    [ "$div" = "1" ] && add_finding "URGENT" "rahzerd"         "XIAOMI CONNECTIVITY DIVERGENCE DETECTED"         "Xiaomi network behaviour deviated from baseline — possible telemetry spike."         "april_oneil.sh" "Refresh Channel 6"
}

parse_granitord() {
    local f="$RESULTS/granitord.json"; [ ! -f "$f" ] && return
    local score grade selinux vboot root drift_detected
    score=$(jq -r '.posture.score // 100' "$f" 2>/dev/null)
    grade=$(jq -r '.posture.grade // "SECURE"' "$f" 2>/dev/null)
    selinux=$(jq -r '.posture.selinux // "unknown"' "$f" 2>/dev/null)
    vboot=$(jq -r '.posture.verified_boot_state // "unknown"' "$f" 2>/dev/null)
    root=$(jq -r '.posture.root_present // false' "$f" 2>/dev/null)
    drift_detected=$(jq -r '.drift.detected // false' "$f" 2>/dev/null)

    [ "${score:-100}" -lt 70 ] && add_finding "URGENT" "granitord"         "SECURITY POSTURE ${score}/100 — ${grade}"         "System security posture degraded. SELinux: ${selinux}, Boot: ${vboot}."         "kernel_sanders.sh" "Full security audit"

    [ "$root" = "true" ] && add_finding "WARNING" "granitord"         "ROOT PRESENCE DETECTED"         "Root indicators found on device. Verify this is intentional."         "StalkerSlayer.sh" "Check root indicators"

    [ "$drift_detected" = "true" ] && add_finding "WARNING" "granitord"         "SECURITY PARAMETER DRIFT"         "Kernel security parameters changed since baseline."         "kernel_sanders.sh" "Parameter audit"
}

parse_ratkingd() {
    local f="$RESULTS/ratkingd.json"; [ ! -f "$f" ] && return
    local total zombies hidden orphans mem_low pressure
    total=$(jq -r '.processes.total // 0' "$f" 2>/dev/null)
    zombies=$(jq -r '.processes.zombies // 0' "$f" 2>/dev/null)
    hidden=$(jq -r '.processes.hidden_gap // 0' "$f" 2>/dev/null)
    orphans=$(jq -r '.processes.orphans // 0' "$f" 2>/dev/null)
    mem_low=$(jq -r '.pressure.memory_low // false' "$f" 2>/dev/null)
    pressure=$(jq -r '.pressure.avail_mb // 0' "$f" 2>/dev/null)

    [ "${zombies:-0}" -gt 5 ] && add_finding "WARNING" "ratkingd"         "${zombies} ZOMBIE PROCESSES"         "High zombie count may indicate process management issues."         "ram_slammer_v2.sh" "Process scan (option 2)"

    [ "$mem_low" = "true" ] && add_finding "URGENT" "ratkingd"         "MEMORY PRESSURE — ${pressure}MB AVAILABLE"         "Device memory critically low. Performance and stability at risk."         "ram_slammer_v2.sh" "Memory summary (option 3)"

    [ "${orphans:-0}" -gt 3 ] && add_finding "WARNING" "ratkingd"         "${orphans} ORPHAN PROCESSES DETECTED"         "Processes with no parent — possible injection or crash remnants."         "ram_slammer_v2.sh" "Process investigation (option 2)"
}

parse_leatherheadd() {
    local f="$RESULTS/leatherheadd.json"; [ ! -f "$f" ] && return
    local score grade throttled
    score=$(jq -r '.thermal_score // 100' "$f" 2>/dev/null)
    grade=$(jq -r '.grade // "NOMINAL"' "$f" 2>/dev/null)
    throttled=$(jq -r '.throttled_cores // 0' "$f" 2>/dev/null)

    [ "${throttled:-0}" -gt 2 ] && add_finding "WARNING" "leatherheadd" \
        "${throttled} CORES THROTTLING — RUNNING HOT" \
        "CPU thermal throttling active. Performance degraded to prevent damage." \
        "hot_counter.sh" "Monitor live temperatures (option 4)"

    [ "${score:-100}" -lt 60 ] && add_finding "URGENT" "leatherheadd" \
        "CRITICAL THERMAL EVENT — SCORE ${score}/100" \
        "Device temperature critical. Immediate action required." \
        "hot_counter.sh" "Live HAL temperatures (option 3)"
}

parse_rocksteadyd() {
    local f="$RESULTS/rocksteadyd.json"; [ ! -f "$f" ] && return
    local score grade throttled
    score=$(jq -r '.cpu_score // 100' "$f" 2>/dev/null)
    grade=$(jq -r '.grade // "HEALTHY"' "$f" 2>/dev/null)
    throttled=$(jq -r '.throttled_cores // 0' "$f" 2>/dev/null)

    [ "${throttled:-0}" -gt 4 ] && add_finding "WARNING" "rocksteadyd" \
        "${throttled} CORES FREQUENCY-THROTTLED" \
        "CPU scaling down under thermal or power stress." \
        "freq_like_me.sh" "Live CPU frequencies (option 2)"
}

parse_bebopd() {
    local f="$RESULTS/bebopd.json"; [ ! -f "$f" ] && return
    local drain
    drain=$(jq -r '.drain_mah_per_hour // 0' "$f" 2>/dev/null)
    local drain_int=${drain%.*}
    [ "${drain_int:-0}" -gt 400 ] && add_finding "WARNING" "bebopd" \
        "BATTERY DRAINING AT ${drain} mAh/h" \
        "Abnormal power draw. Wakelocks or aggressive background activity suspected." \
        "update_blocker.sh" "Check background activity"
}

parse_fugitoidd() {
    local f="$RESULTS/fugitoidd.json"; [ ! -f "$f" ] && return
    local crashes anrs ooms
    crashes=$(jq -r '.crash_count // 0' "$f" 2>/dev/null)
    anrs=$(jq -r '.anr_count // 0' "$f" 2>/dev/null)
    ooms=$(jq -r '.oom_count // 0' "$f" 2>/dev/null)

    [ "${ooms:-0}" -gt 0 ] && add_finding "URGENT" "fugitoidd" \
        "${ooms} OUT-OF-MEMORY KILL EVENTS" \
        "System critically low on RAM. Processes being killed to survive." \
        "ram_slammer_v2.sh" "Emergency RAM check"

    [ "${crashes:-0}" -gt 0 ] && add_finding "WARNING" "fugitoidd" \
        "${crashes} APP CRASHES DETECTED" \
        "Recent application crashes logged. System may be unstable." \
        "ram_slammer_v2.sh" "Check memory pressure"

    [ "${anrs:-0}" -gt 0 ] && add_finding "WARNING" "fugitoidd" \
        "${anrs} ANR EVENTS DETECTED" \
        "App Not Responding events. Possible CPU starvation or memory pressure." \
        "ram_slammer_v2.sh" "Check top memory consumers"
}

# ── Render findings ────────────────────────────────────────────────────────────
render_findings() {
    local urgent=0 warnings=0 notices=0

    for entry in "${FINDINGS[@]}"; do
        local sev; sev=$(echo "$entry" | cut -d'|' -f1)
        case "$sev" in URGENT) urgent=$((urgent+1));; WARNING) warnings=$((warnings+1));; NOTICE) notices=$((notices+1));; esac
    done

    # Summary block
    if [ $urgent -gt 0 ]; then
        block_head "$BGRED" "$WHITE" "BREAKING: ${urgent} URGENT ALERT(S) REQUIRE IMMEDIATE ACTION"
    elif [ $warnings -gt 0 ]; then
        block_head "$BGYEL" "$BLACK" "TONIGHT: ${warnings} WARNING(S) FLAGGED FOR REVIEW"
    else
        block_head "$BGCYAN" "$BLACK" "ALL CLEAR — NO ACTIONABLE FINDINGS"
    fi
    echo ""

    if [ ${#FINDINGS[@]} -eq 0 ]; then
        printf " ${INK}No findings from last scan cycle. All systems nominal.${RESET}\n\n"
        return
    fi

    local sep="|||"

    # URGENT
    for entry in "${FINDINGS[@]}"; do
        local sev daemon headline detail tool action
        sev=$(echo "$entry" | awk -F'[|][|][|]' '{print $1}')
        [ "$sev" != "URGENT" ] && continue
        daemon=$(echo "$entry" | awk -F'[|][|][|]' '{print $2}')
        headline=$(echo "$entry" | awk -F'[|][|][|]' '{print $3}')
        detail=$(echo "$entry" | awk -F'[|][|][|]' '{print $4}')
        tool=$(echo "$entry" | awk -F'[|][|][|]' '{print $5}')
        action=$(echo "$entry" | awk -F'[|][|][|]' '{print $6}')

        echo ""
        block_head "$BGRED" "$WHITE" "!! $headline !!"
        printf " ${GREY}SOURCE: %-12s${RESET}\n" "$daemon"
        printf " ${INK}%s${RESET}\n" "$detail"
        printf " ${BOLD}${YEL}>> %s${RESET} ${GREY}— %s${RESET}\n" "$tool" "$action"
        rule
    done

    # WARNING
    for entry in "${FINDINGS[@]}"; do
        local sev daemon headline detail tool action
        sev=$(echo "$entry" | awk -F'[|][|][|]' '{print $1}')
        [ "$sev" != "WARNING" ] && continue
        daemon=$(echo "$entry" | awk -F'[|][|][|]' '{print $2}')
        headline=$(echo "$entry" | awk -F'[|][|][|]' '{print $3}')
        detail=$(echo "$entry" | awk -F'[|][|][|]' '{print $4}')
        tool=$(echo "$entry" | awk -F'[|][|][|]' '{print $5}')
        action=$(echo "$entry" | awk -F'[|][|][|]' '{print $6}')

        echo ""
        block_head "$BGYEL" "$BLACK" "$headline"
        printf " ${GREY}SOURCE: %-12s${RESET}\n" "$daemon"
        printf " ${INK}%s${RESET}\n" "$detail"
        printf " ${BOLD}${CYAN}>> %s${RESET} ${GREY}— %s${RESET}\n" "$tool" "$action"
        rule
    done

    # NOTICE
    for entry in "${FINDINGS[@]}"; do
        local sev daemon headline detail tool action
        sev=$(echo "$entry" | awk -F'[|][|][|]' '{print $1}')
        [ "$sev" != "NOTICE" ] && continue
        daemon=$(echo "$entry" | awk -F'[|][|][|]' '{print $2}')
        headline=$(echo "$entry" | awk -F'[|][|][|]' '{print $3}')
        detail=$(echo "$entry" | awk -F'[|][|][|]' '{print $4}')
        tool=$(echo "$entry" | awk -F'[|][|][|]' '{print $5}')
        action=$(echo "$entry" | awk -F'[|][|][|]' '{print $6}')

        echo ""
        block_head "$BGCYAN" "$BLACK" "$headline"
        printf " ${GREY}SOURCE: %-12s${RESET}\n" "$daemon"
        printf " ${INK}%s${RESET}\n" "$detail"
        printf " ${BOLD}${GREY}>> %s${RESET} ${GREY}— %s${RESET}\n" "$tool" "$action"
        rule
    done
    echo ""
}

# ── Bureau status ──────────────────────────────────────────────────────────────
render_status() {
    block_head "$BGBLUE" "$WHITE" "BUREAU STATUS — DAEMON HEALTH"
    echo ""
    local daemons="gaveld burned granitord leatherheadd rocksteadyd bebopd rahzerd ratkingd metalheadd shredderd fugitoidd krangd turtlecomd splinterd"
    for d in $daemons; do
        local result="$RESULTS/${d}.json"
        local running ts
        pgrep -x "$d" > /dev/null 2>&1 && running="${CYAN}● LIVE  ${RESET}" || running="${GREY}○ idle  ${RESET}"
        if [ -f "$result" ]; then
            ts=$(jq -r '.timestamp // "?"' "$result" 2>/dev/null | cut -dT -f2 | cut -d: -f1,2)
            printf " %b${INK}%-16s${RESET} ${GREY}last scan: %s${RESET}\n" "$running" "$d" "$ts"
        else
            printf " %b${INK}%-16s${RESET} ${GREY}no results yet${RESET}\n" "$running" "$d"
        fi
    done
    echo ""
}

# ── Launch tool ────────────────────────────────────────────────────────────────
launch_tool() {
    local tools_list=(
        "StalkerSlayer.sh|Telemetry Blocker"
        "multi_sensory_room.sh|Sensor Registry"
        "kernel_sanders.sh|Kernel Integrity"
        "hot_counter.sh|Thermal Truth"
        "freq_like_me.sh|CPU Frequencies"
        "hidden_settings.sh|MIUI Hidden Settings"
        "network_spy.sh|Network Audit"
        "ram_slammer_v2.sh|Memory + Process"
        "_id_checker.sh|Identity Audit"
        "discover.sh|Privacy Auditor"
        "update_blocker.sh|OTA Blocker"
    )

    block_head "$BGBLUE" "$WHITE" "CHANNEL 6 TOOL DESK"
    echo ""
    local i=1
    for entry in "${tools_list[@]}"; do
        local name action
        name=$(echo "$entry" | cut -d'|' -f1)
        action=$(echo "$entry" | cut -d'|' -f2)
        printf " ${BOLD}${YEL}[%2d]${RESET} ${INK}%-30s${RESET} ${GREY}%s${RESET}\n" $i "$name" "$action"
        i=$((i+1))
    done
    printf " ${GREY}[ q] Back${RESET}\n\n"
    read -r -p "$(echo -e " ${CYAN}Choice: ${RESET}")" pick
    [ "$pick" = "q" ] && return

    local idx=$((pick - 1))
    local selected="${tools_list[$idx]}"
    [ -z "$selected" ] && return
    local toolname; toolname=$(echo "$selected" | cut -d'|' -f1)
    [ -f "$TOOLS/$toolname" ] && bash "$TOOLS/$toolname" || { echo -e " ${RED}Tool not found: $toolname${RESET}"; sleep 2; }
}

# ── Menu ───────────────────────────────────────────────────────────────────────
menu() {
    hrule
    echo ""
    printf " ${BOLD}${WHITE}[1]${RESET} ${INK}Full briefing${RESET}\n"
    printf " ${WHITE}[2]${RESET} ${INK}Bureau status${RESET}\n"
    printf " ${WHITE}[3]${RESET} ${INK}Launch a tool${RESET}\n"
    printf " ${WHITE}[4]${RESET} ${INK}Refresh${RESET}\n"
    printf " ${GREY}[q] Quit${RESET}\n\n"
    read -r -p "$(echo -e " ${CYAN}Choice: ${RESET}")" choice
    case "$choice" in
        1) masthead; run_parsers; render_findings; menu ;;
        2) masthead; render_status; menu ;;
        3) launch_tool; masthead; run_parsers; render_findings; menu ;;
        4) main ;;
        q|Q) exit 0 ;;
        *) menu ;;
    esac
}

run_parsers() {
    FINDINGS=()
    parse_burned; parse_metalheadd; parse_shredderd; parse_granitord; parse_ratkingd; parse_rahzerd
    parse_leatherheadd; parse_rocksteadyd; parse_bebopd; parse_fugitoidd
}

main() {
    masthead
    run_parsers
    render_findings
    render_status
    menu
}

main
