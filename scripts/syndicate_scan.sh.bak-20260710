#!/data/data/com.termux/files/usr/bin/bash
# syndicate_scan.sh — MiuiserPeruser Full Syndicate Scan
# Consent-gated, toggle-aware, once or looped

BASE="$HOME/MiuiserPeruser"
BIN="$BASE/bin"
RESULTS="$BASE/Registry/daemon_results"
LOGS="$BASE/logs"
CONSENT_LOCK="$BASE/data/.syndicate_consent.lock"
CONFIG="$BASE/data/syndicate_config.json"
mkdir -p "$LOGS" "$RESULTS"

# ── Colours ──────────────────────────────────────────────────────────────────
RED='\033[41m\033[97m'; YEL='\033[43m\033[30m'; GRN='\033[32m'
CYN='\033[36m'; WHT='\033[97m'; DIM='\033[2m'; BOLD='\033[1m'; RST='\033[0m'

# ── Default daemon fleet ─────────────────────────────────────────────────────
ALL_DAEMONS="burned granitord leatherheadd metalheadd rahzerd ratkingd rocksteadyd shredderd tigerclawd bebopd fugitoidd overlordd"

# ── Load enabled daemons from config ─────────────────────────────────────────
load_config() {
    if [ -f "$CONFIG" ]; then
        ENABLED=$(jq -r '.enabled_daemons[]' "$CONFIG" 2>/dev/null | tr '\n' ' ')
        [ -z "$ENABLED" ] && ENABLED="$ALL_DAEMONS"
    else
        ENABLED="$ALL_DAEMONS"
        save_config
    fi
}

save_config() {
    local json='{"enabled_daemons":['
    local first=1
    for d in $ENABLED; do
        [ $first -eq 0 ] && json+=','
        json+="\"$d\""
        first=0
    done
    json+=']}'
    echo "$json" > "$CONFIG"
}

# ── Consent gate ─────────────────────────────────────────────────────────────
consent_gate() {
    [ -f "$CONSENT_LOCK" ] && return 0
    clear
    printf "${BOLD}${WHT}"
    printf "╔══════════════════════════════════════════════╗\n"
    printf "║     MIUISERPERUSER — SYNDICATE SCAN MODE     ║\n"
    printf "║          Full Device Intelligence Scan       ║\n"
    printf "╚══════════════════════════════════════════════╝\n"
    printf "${RST}\n"
    printf "${WHT}  WHAT THIS SCAN READS:\n\n"
    printf "${CYN}  • Kernel modules and integrity state\n"
    printf "  • Network connections and port listeners\n"
    printf "  • Running processes and memory pressure\n"
    printf "  • Thermal state and CPU frequencies\n"
    printf "  • System properties and telemetry flags\n"
    printf "  • Binder service registry topology\n"
    printf "  • Battery drain and wakelock activity\n\n"
    printf "${WHT}  WHAT GETS STORED:\n\n"
    printf "${CYN}  • Results saved to Registry/daemon_results/\n"
    printf "  • Nothing transmitted off-device\n"
    printf "  • Purge anytime: rm $RESULTS/*.json\n\n"
    printf "${WHT}  GDPR NOTICE: This tool processes data about\n"
    printf "  THIS DEVICE ONLY for personal security research.\n\n"
    printf "══════════════════════════════════════════════\n\n"
    printf "  Type ${YEL}I CONSENT${RST} to continue: "
    read -r input
    if [ "$input" != "I CONSENT" ]; then
        printf "\n  Consent not given. Exiting.\n\n"
        exit 0
    fi
    touch "$CONSENT_LOCK"
    printf "\n  ${GRN}✔ Consent recorded.${RST}\n\n"
    sleep 1
}

# ── Run a single daemon ───────────────────────────────────────────────────────
run_daemon() {
    local d=$1
    local bin="$BIN/$d"
    [ ! -x "$bin" ] && printf "  ${DIM}%-14s skipped (no binary)${RST}\n" "$d" && return

    printf "  ${CYN}%-14s${RST} scanning..." "$d"
    RISH_APPLICATION_ID=com.termux timeout 60 "$bin" >> "$LOGS/${d}.log" 2>&1 &
    local pid=$!
    # Wait for result file to appear or timeout
    local waited=0
    while [ $waited -lt 55 ]; do
        [ -f "$RESULTS/${d}.json" ] && break
        sleep 2; waited=$((waited+2))
    done
    wait $pid 2>/dev/null
    if [ -f "$RESULTS/${d}.json" ]; then
        printf " ${GRN}✔${RST}\n"
    else
        printf " ${DIM}no results${RST}\n"
    fi
}

# ── Run full scan ─────────────────────────────────────────────────────────────
run_scan() {
    local round=$1
    clear
    printf "\n${BOLD}${WHT}  SYNDICATE SCAN — ROUND ${round}${RST}\n"
    printf "  ${DIM}$(date '+%Y-%m-%d %H:%M:%S')${RST}\n\n"

    # Pre-warm granitord (slow daemon — run first)
    rm -f "$RESULTS/granitord.json"
    printf "  ${CYN}%-14s${RST} ${DIM}pre-scanning...${RST}\n" "granitord"
    GRANITORD_POLL_SEC=5 RISH_APPLICATION_ID=com.termux timeout 120 "$BIN/granitord" >> "$LOGS/granitord.log" 2>&1 &
    local gran_pid=$!

    # Clear other results
    for d in $ENABLED; do [ "$d" != "granitord" ] && rm -f "$RESULTS/${d}.json"; done

    # Run dump_rahzerd pre-bake first
    [ -x "$BASE/scripts/dump_rahzerd.sh" ] && "$BASE/scripts/dump_rahzerd.sh" 2>/dev/null

    # Run enabled daemons in parallel batches
    local pids=()
    for d in $ENABLED; do
        local bin="$BIN/$d"
        [ ! -x "$bin" ] && continue
        printf "  ${CYN}%-14s${RST} ${DIM}starting...${RST}\n" "$d"
        local timeout_sec=90
        local extra_env=""
        [ "$d" = "granitord" ] && timeout_sec=120 && extra_env="GRANITORD_POLL_SEC=5"
        [ "$d" = "fugitoidd" ] && extra_env="$extra_env BEXEC_NO_RISH=1"
        RISH_APPLICATION_ID=com.termux $extra_env timeout $timeout_sec "$bin" >> "$LOGS/${d}.log" 2>&1 &
        pids+=($!)
    done

    # Wait and show progress
    printf "\n  ${DIM}Waiting for results...${RST}\n"
    wait $gran_pid 2>/dev/null || true
    local done_count=0
    local total=${#pids[@]}
    for pid in "${pids[@]}"; do
        wait $pid 2>/dev/null
        done_count=$((done_count+1))
        printf "\r  ${GRN}✔${RST} ${done_count}/${total} complete"
    done
    printf "\n\n"

    # Show meaty summary
    printf "  ${BOLD}${WHT}SCAN RESULTS:${RST}\n"
    printf "${CYN}%48s${RST}\n" | tr ' ' '='
    for d in $ENABLED; do
        local f="$RESULTS/${d}.json"
        if [ -f "$f" ]; then
            local ts=$(jq -r '.timestamp // "?"' "$f" 2>/dev/null | grep -oE '[0-9]{2}:[0-9]{2}' | head -1)
            printf "\n  ${GRN}✔${RST} ${BOLD}${CYN}%s${RST} ${DIM}@ %s${RST}\n" "$d" "$ts"
            case "$d" in
                burned)
                    local sigs=$(jq -r '.privacy_signal_count // 0' "$f" 2>/dev/null)
                    local list=$(jq -r '.privacy_list // ""' "$f" 2>/dev/null | tr ',' ' ' | xargs)
                    printf "    ${WHT}Privacy signals: ${YEL}%s detected${RST}\n" "$sigs"
                    [ -n "$list" ] && printf "    ${DIM}%s${RST}\n" "${list:0:44}"
                    printf "\n    ${BOLD}${WHT}WHAT THIS MEANS:${RST}\n"
                    printf "    ${WHT}Your phone has tracking code baked into it at\n"
                    printf "    factory level by Xiaomi. This isn't something\n"
                    printf "    you installed — it's built into the firmware.\n"
                    printf "    Like buying a car and finding the dealer already\n"
                    printf "    fitted a GPS tracker you didn't ask for.${RST}\n"
                    printf "\n    ${BOLD}${CYN}TECHNICAL:${RST}\n"
                    printf "    ${DIM}Partner/advertiser IDs (Facebook, AppsFlyer)\n"
                    printf "    embedded at ROM level. Cannot be removed without\n"
                    printf "    flashing. Verify persist.sys.miui_optimization\n"
                    printf "    and GDPR opt-out flags on EEA build.${RST}\n"
                    ;;
                granitord)
                    local score=$(jq -r '.posture.score // "?"' "$f" 2>/dev/null)
                    local grade=$(jq -r '.posture.grade // "?"' "$f" 2>/dev/null)
                    local selinux=$(jq -r '.posture.selinux // "?"' "$f" 2>/dev/null)
                    printf "    ${WHT}Security posture: ${YEL}%s/100 — %s${RST}\n" "$score" "$grade"
                    printf "    ${WHT}SELinux: ${YEL}%s${RST}\n" "$selinux"
                    printf "\n    ${BOLD}${WHT}WHAT THIS MEANS:${RST}\n"
                    printf "    ${WHT}This checks the security foundations of your\n"
                    printf "    device — whether the locks are on and nobody has\n"
                    printf "    tampered with the core OS. A low score means\n"
                    printf "    those foundations are weaker than they should be.${RST}\n"
                    printf "\n    ${BOLD}${CYN}TECHNICAL:${RST}\n"
                    printf "    ${DIM}Kernel security posture: SELinux state, verified\n"
                    printf "    boot chain, ro.secure/ro.debuggable props,\n"
                    printf "    dm-verity status. Score across 7 integrity vectors.${RST}\n"
                    ;;
                shredderd)
                    local score=$(python3 -c "import json,sys; d=json.load(open('$f')); print(d.get('integrity',{}).get('score','?'))" 2>/dev/null || echo "?")
                    local grade=$(python3 -c "import json,sys; d=json.load(open('$f')); print(d.get('integrity',{}).get('grade','?'))" 2>/dev/null || echo "?")
                    local modules=$(python3 -c "import json,sys; d=json.load(open('$f')); print(d.get('baseline',{}).get('module_count','?'))" 2>/dev/null || echo "?")
                    printf "    ${WHT}Kernel integrity: ${YEL}%s/100 — %s${RST}\n" "$score" "$grade"
                    printf "    ${WHT}Modules loaded: ${YEL}%s${RST}\n" "$modules"
                    printf "\n    ${BOLD}${WHT}WHAT THIS MEANS:${RST}\n"
                    printf "    ${WHT}Checks whether the kernel — the deepest part of\n"
                    printf "    the OS — has been modified. Like checking whether\n"
                    printf "    a building's foundations have been tampered with.\n"
                    printf "    Unexpected modules could indicate a rootkit.${RST}\n"
                    printf "\n    ${BOLD}${CYN}TECHNICAL:${RST}\n"
                    printf "    ${DIM}Kernel module inventory vs baseline snapshot.\n"
                    printf "    DJB2 hash of /proc/modules. Flags new modules\n"
                    printf "    not present at baseline. Checks known rootkit\n"
                    printf "    module names and suspicious sysfs exposure.${RST}\n"
                    ;;
                leatherheadd)
                    local score=$(jq -r '.thermal_score // "?"' "$f" 2>/dev/null)
                    local grade=$(jq -r '.grade // "?"' "$f" 2>/dev/null)
                    local throttled=$(jq -r '.throttled_cores // 0' "$f" 2>/dev/null)
                    printf "    ${WHT}Thermal score: ${YEL}%s/100 — %s${RST}\n" "$score" "$grade"
                    printf "    ${WHT}Throttled cores: ${YEL}%s${RST}\n" "$throttled"
                    printf "\n    ${BOLD}${WHT}WHAT THIS MEANS:${RST}\n"
                    printf "    ${WHT}Your phone is generating heat. When it gets too\n"
                    printf "    hot, it slows processor cores to avoid damage —\n"
                    printf "    like a car engine cutting power to prevent seizure.\n"
                    printf "    Critical score means it needs to cool down now.${RST}\n"
                    printf "\n    ${BOLD}${CYN}TECHNICAL:${RST}\n"
                    printf "    ${DIM}MTK thermal zone polling via HAL thermalservice.\n"
                    printf "    Throttling via scaling_cur_freq deltas. Score <60\n"
                    printf "    indicates sustained thermal stress across zones.${RST}\n"
                    ;;
                rahzerd)
                    local wifi=$(jq -r '.wifi.connected // -1' "$f" 2>/dev/null)
                    local rat=$(jq -r '.mobile.rat // "?"' "$f" 2>/dev/null)
                    local dns=$(jq -r '.dns.latency_ms // "?"' "$f" 2>/dev/null)
                    local tcp4=$(jq -r '.ports.established_tcp4 // 0' "$f" 2>/dev/null)
                    local tcp6=$(jq -r '.ports.established_tcp6 // 0' "$f" 2>/dev/null)
                    printf "    ${WHT}WiFi: ${YEL}%s${RST}  ${WHT}Mobile: ${YEL}%s${RST}  ${WHT}DNS: ${YEL}%sms${RST}\n"                         "$([ "$wifi" = "1" ] && echo connected || echo disconnected)" "$rat" "$dns"
                    printf "    ${WHT}Connections: ${YEL}%s TCP4 + %s TCP6${RST}\n" "$tcp4" "$tcp6"
                    printf "\n    ${BOLD}${WHT}WHAT THIS MEANS:${RST}\n"
                    printf "    ${WHT}This monitors all your network connections —\n"
                    printf "    WiFi, mobile data, DNS and open connections.\n"
                    printf "    Think of it as watching all the doors and windows\n"
                    printf "    of your house to see what's coming in and going out.${RST}\n"
                    printf "\n    ${BOLD}${CYN}TECHNICAL:${RST}\n"
                    printf "    ${DIM}Pre-baked connectivity audit via adb_cli. Parses\n"
                    printf "    telephony.registry dumpsys for RAT/registration.\n"
                    printf "    DNS via getaddrinfo(). TCP count from /proc/net/tcp.\n"
                    printf "    Baseline drift via BSSID/DNS latency comparison.${RST}\n"
                    ;;
                ratkingd)
                    local zombies=$(jq -r '.processes.zombies // 0' "$f" 2>/dev/null)
                    local orphans=$(jq -r '.processes.orphans // 0' "$f" 2>/dev/null)
                    local avail=$(jq -r '.memory.available_mb // .pressure.avail_mb // "?"' "$f" 2>/dev/null)
                    printf "    ${WHT}Memory available: ${YEL}%s MB${RST}\n" "$avail"
                    printf "    ${WHT}Zombies: ${YEL}%s${RST}  ${WHT}Orphans: ${YEL}%s${RST}\n" "$zombies" "$orphans"
                    printf "\n    ${BOLD}${WHT}WHAT THIS MEANS:${RST}\n"
                    printf "    ${WHT}Watches your phone's memory and processes.\n"
                    printf "    Zombie processes are programs that crashed but\n"
                    printf "    haven't shut down — like a ghost still haunting\n"
                    printf "    the house. High counts make your phone unstable.${RST}\n"
                    printf "\n    ${BOLD}${CYN}TECHNICAL:${RST}\n"
                    printf "    ${DIM}Parses /proc/[pid]/status for zombie state (Z).\n"
                    printf "    Orphan detection via PPID=1 scan. Memory pressure\n"
                    printf "    from /proc/meminfo MemAvailable. OOM kills via\n"
                    printf "    logcat Low on memory/Kill entries.${RST}\n"
                    ;;
                tigerclawd)
                    local trust=$(jq -r '.trust_score // "?"' "$f" 2>/dev/null)
                    local svcs=$(jq -r '.binder.service_count // "?"' "$f" 2>/dev/null)
                    local drift=$(jq -r '.binder.drift // 0' "$f" 2>/dev/null)
                    local ver=$(jq -r '.device.hyperos_version // "?"' "$f" 2>/dev/null)
                    local patch=$(jq -r '.device.security_patch // "?"' "$f" 2>/dev/null)
                    printf "    ${WHT}Trust score: ${YEL}%s/100${RST}  ${WHT}HyperOS: ${YEL}%s${RST}\n" "$trust" "$ver"
                    printf "    ${WHT}Services: ${YEL}%s${RST}  ${WHT}Drift: ${YEL}%s${RST}  ${WHT}Patch: ${YEL}%s${RST}\n" "$svcs" "$drift" "$patch"
                    printf "\n    ${BOLD}${WHT}WHAT THIS MEANS:${RST}\n"
                    printf "    ${WHT}The eye inside HyperOS — learns what your device\n"
                    printf "    normally looks like and flags anything that deviates.\n"
                    printf "    Like a security guard who knows every staff member\n"
                    printf "    and immediately spots a stranger in the building.${RST}\n"
                    printf "\n    ${BOLD}${CYN}TECHNICAL:${RST}\n"
                    printf "    ${DIM}Binder service registry topology via adb_cli.\n"
                    printf "    Baseline fingerprints 473 services at first run.\n"
                    printf "    DJB2 hash of ro.* props for drift detection.\n"
                    printf "    Trust score across 6 integrity vectors.${RST}\n"
                    ;;
                metalheadd)
                    local score=$(jq -r '.sensor_score // "?"' "$f" 2>/dev/null)
                    local active=$(jq -r '.sensitive_active // 0' "$f" 2>/dev/null)
                    printf "    ${WHT}Sensor score: ${YEL}%s/100${RST}  ${WHT}Sensitive active: ${YEL}%s${RST}\n" "$score" "$active"
                    printf "\n    ${BOLD}${WHT}WHAT THIS MEANS:${RST}\n"
                    printf "    ${WHT}Watches your phone's sensors — accelerometer,\n"
                    printf "    gyroscope, microphone access. Apps shouldn't poll\n"
                    printf "    movement sensors in the background. If they are,\n"
                    printf "    something may be tracking you without your knowledge.${RST}\n"
                    printf "\n    ${BOLD}${CYN}TECHNICAL:${RST}\n"
                    printf "    ${DIM}Sensor access via /proc/[pid]/fd scanning for\n"
                    printf "    sensor device nodes. Sensitive classification:\n"
                    printf "    ACCELEROMETER, GYROSCOPE, MAGNETIC_FIELD.\n"
                    printf "    Background access flagged without foreground activity.${RST}\n"
                    ;;
                rocksteadyd)
                    local score=$(jq -r '.cpu_score // "?"' "$f" 2>/dev/null)
                    local throttled=$(jq -r '.throttled_cores // 0' "$f" 2>/dev/null)
                    printf "    ${WHT}CPU score: ${YEL}%s/100${RST}  ${WHT}Throttled: ${YEL}%s cores${RST}\n" "$score" "$throttled"
                    printf "\n    ${BOLD}${WHT}WHAT THIS MEANS:${RST}\n"
                    printf "    ${WHT}Monitors how hard your processor is working.\n"
                    printf "    When cores throttle down your phone slows noticeably.\n"
                    printf "    Sustained throttling means something is pushing\n"
                    printf "    the hardware too hard in the background.${RST}\n"
                    printf "\n    ${BOLD}${CYN}TECHNICAL:${RST}\n"
                    printf "    ${DIM}Per-core frequency via scaling_cur_freq. Throttle\n"
                    printf "    detected when freq drops below 60%% of max_freq\n"
                    printf "    for >3 consecutive polls. Score by core count\n"
                    printf "    and throttle depth.${RST}\n"
                    ;;
                bebopd)
                    local drain=$(jq -r '.drain_mah_h // "?"' "$f" 2>/dev/null)
                    local health=$(jq -r '.grade // "?"' "$f" 2>/dev/null)
                    printf "    ${WHT}Drain: ${YEL}%s mAh/hr${RST}  ${WHT}Health: ${YEL}%s${RST}\n" "$drain" "$health"
                    printf "\n    ${BOLD}${WHT}WHAT THIS MEANS:${RST}\n"
                    printf "    ${WHT}Measures how fast your battery is draining.\n"
                    printf "    A healthy phone at rest should barely use power.\n"
                    printf "    Fast drain when idle means something is running\n"
                    printf "    in the background — apps, telemetry, or worse.${RST}\n"
                    printf "\n    ${BOLD}${CYN}TECHNICAL:${RST}\n"
                    printf "    ${DIM}Drain via /sys/class/power_supply/battery/\n"
                    printf "    charge_now delta over poll interval. mAh/hr from\n"
                    printf "    current_now. Drain >400mAh/hr at idle is anomalous.${RST}\n"
                    ;;
                fugitoidd)
                    local crashes=$(jq -r '.crashes // .crash_count // 0' "$f" 2>/dev/null)
                    local anrs=$(jq -r '.anrs // .anr_count // 0' "$f" 2>/dev/null)
                    local ooms=$(jq -r '.oom_events // .oom_count // 0' "$f" 2>/dev/null)
                    printf "    ${WHT}Crashes: ${YEL}%s${RST}  ${WHT}ANRs: ${YEL}%s${RST}  ${WHT}OOMs: ${YEL}%s${RST}\n" "$crashes" "$anrs" "$ooms"
                    printf "\n    ${BOLD}${WHT}WHAT THIS MEANS:${RST}\n"
                    printf "    ${WHT}Reads your system crash logs. App crashes, ANRs\n"
                    printf "    (App Not Responding freezes) and out-of-memory kills\n"
                    printf "    all leave traces. Frequent crashes mean your phone\n"
                    printf "    is running too hard or has a misbehaving app.${RST}\n"
                    printf "\n    ${BOLD}${CYN}TECHNICAL:${RST}\n"
                    printf "    ${DIM}Logcat parsing for FATAL EXCEPTION, ANR in,\n"
                    printf "    Low on memory/Kill entries. OOM kill events\n"
                    printf "    from kernel log. Timestamps correlated against\n"
                    printf "    process table for attribution.${RST}\n"
                    ;;
            esac
            printf "${CYN}%48s${RST}\n" | tr ' ' '-'
        else
            printf "\n  ${DIM}○ %-14s no results${RST}\n" "$d"
        fi
    done
    printf "\n${CYN}%48s${RST}\n" | tr ' ' '='
    printf "\n  ${DIM}Press any key to return to menu...${RST}"
    read -n1
    main
}

# ── Live view ─────────────────────────────────────────────────────────────────
live_view() {
    clear
    printf "${BOLD}${WHT}  SYNDICATE LIVE VIEW${RST}  ${DIM}Ctrl+C to exit${RST}\n"
    printf "${CYN}%*s${RST}\n" 48 | tr ' ' '='

    while true; do
        # Move cursor to line 3
        printf "\033[3;1H"
        local now=$(date '+%H:%M:%S')
        printf "${BOLD}${WHT}  SYNDICATE LIVE VIEW${RST}  ${DIM}%s${RST}        \n" "$now"
        printf "${CYN}%*s${RST}\n" 48 | tr ' ' '='

        for d in $ENABLED; do
            local f="$RESULTS/${d}.json"
            if [ -f "$f" ]; then
                local ts=$(jq -r '.timestamp // "?"' "$f" 2>/dev/null | grep -oE '[0-9]{2}:[0-9]{2}' | head -1)
                local extra=""
                # Daemon-specific live metric
                case "$d" in
                    leatherheadd) extra=$(jq -r '.thermal_score // "?"' "$f" 2>/dev/null | awk '{printf "score=%s",$1}') ;;
                    rahzerd)      extra=$(jq -r '"\(.wifi.connected) wifi \(.ports.established_tcp4)tcp"' "$f" 2>/dev/null) ;;
                    tigerclawd)   extra=$(jq -r '"trust=\(.trust_score)"' "$f" 2>/dev/null) ;;
                    ratkingd)     extra=$(jq -r '"zombies=\(.processes.zombies)"' "$f" 2>/dev/null) ;;
                    granitord)    extra=$(jq -r '"\(.posture.grade // "?")"' "$f" 2>/dev/null) ;;
                    shredderd)    extra=$(jq -r '"\(.integrity.grade // "?")"' "$f" 2>/dev/null) ;;
                    burned)       extra=$(jq -r '"signals=\(.privacy_signal_count // 0)"' "$f" 2>/dev/null) ;;
                    bebopd)       extra=$(jq -r '"drain=\(.drain_mah_per_hour // 0)mAh"' "$f" 2>/dev/null) ;;
                esac
                printf "  ${GRN}●${RST} ${CYN}%-14s${RST} ${DIM}%s${RST} ${WHT}%s${RST}          \n" "$d" "$ts" "$extra"
            else
                printf "  ${DIM}○ %-14s no data${RST}          \n" "$d"
            fi
        done
        sleep 3
    done
}

# ── Main ──────────────────────────────────────────────────────────────────────
main() {
    consent_gate
    load_config

    printf "\n${BOLD}${WHT}  SYNDICATE SCAN OPTIONS:${RST}\n\n"
    printf "  ${CYN}[1]${RST} ${WHT}Single scan${RST}\n"
    printf "  ${CYN}[2]${RST} ${WHT}Loop N times${RST}\n"
    printf "  ${CYN}[3]${RST} ${WHT}Continuous loop${RST}\n"
    printf "  ${CYN}[4]${RST} ${WHT}Live view${RST}\n"
    printf "  ${CYN}[q]${RST} ${WHT}Quit${RST}\n\n"
    read -r -p "$(printf "  ${CYN}Choice: ${RST}")" choice

    case "$choice" in
        1)
            run_scan 1
            printf "  ${CYN}Press any key to exit...${RST}\n"
            read -n1
            ;;
        2)
            read -r -p "$(printf "  ${CYN}How many times? ${RST}")" n
            for i in $(seq 1 "$n"); do
                run_scan "$i"
                [ "$i" -lt "$n" ] && sleep 5
            done
            printf "  ${GRN}✔ ${n} scans complete.${RST}\n"
            read -n1
            ;;
        3)
            local round=1
            while true; do
                run_scan "$round"
                round=$((round+1))
                printf "  ${DIM}Next scan in 30s — press Ctrl+C to stop${RST}\n"
                sleep 30
            done
            ;;
        4) live_view ;;
        q|Q) exit 0 ;;
    esac
}

main
