#!/data/data/com.termux/files/usr/bin/bash
# ═══════════════════════════════════════════════════════════════════════════════
# syndicate_scan.sh — MiuiserPeruser Full Syndicate Scan (amalgamated)
#
# This replaces two divergent copies that existed side by side:
#   - root ~/MiuiserPeruser/syndicate_scan.sh   (had: bexec-cache-aware parallel
#     rolling_scan, fixed `env $extra` bug, fixed Ctrl+C trap, shredderd single-
#     jq parser reading the correct .drift.new_modules path, nulld wired in,
#     paged prev/next results viewer)
#   - scripts/syndicate_scan.sh                 (the ACTUAL live entrypoint,
#     called by miuiserperuser.sh and syndicate_dashboard.sh — had: richer
#     inline WHAT-THIS-MEANS/TECHNICAL prose, live_view dashboard, granitord
#     pre-warm + dump_rahzerd.sh pre-bake step, but still had the old
#     unfixed $extra_env word-splitting bug and shredderd's 4x cold python3
#     spawn parser)
#
# This file is the merge: live entrypoint location, all bugfixes, both
# feature sets. The old root copy should be deleted once this is confirmed
# working — see handover notes.
# ═══════════════════════════════════════════════════════════════════════════════

BASE="$HOME/MiuiserPeruser"
BIN="$BASE/bin"
RESULTS="$BASE/Registry/daemon_results"
LOGS="$BASE/logs"
CONSENT_LOCK="$BASE/data/.syndicate_consent.lock"
CONFIG="$BASE/data/syndicate_config.json"
SCRIPTS="$BASE/scripts"

mkdir -p "$LOGS" "$RESULTS"

# ── Colours ────────────────────────────────────────────────────────────────────
RED='\033[41m\033[97m'
YEL='\033[43m\033[30m'
GRN='\033[32m'
CYN='\033[36m'
WHT='\033[97m'
DIM='\033[2m'
BOLD='\033[1m'
RST='\033[0m'
MAG='\033[35m'
BLU='\033[34m'
ORN='\033[33m'

# ── Default daemon fleet ───────────────────────────────────────────────────────
ALL_DAEMONS="burned granitord leatherheadd metalheadd rahzerd ratkingd rocksteadyd shredderd tigerclawd bebopd fugitoidd overlordd nulld"

declare -A DAEMON_NAME=(
  [burned]="Burned" [granitord]="Granitord" [leatherheadd]="Leatherhead"
  [metalheadd]="Metalhead" [rahzerd]="Rahzerd" [ratkingd]="Ratkingd"
  [rocksteadyd]="Rocksteady" [shredderd]="Shredderd" [tigerclawd]="Tigerclaw"
  [bebopd]="Bebop" [fugitoidd]="Fugitoid" [overlordd]="Overlord" [nulld]="Nulld"
)

declare -A DAEMON_TIMEOUT=(
  [burned]=30 [granitord]=120 [leatherheadd]=30 [metalheadd]=30
  [rahzerd]=60 [ratkingd]=60 [rocksteadyd]=30 [shredderd]=60
  [tigerclawd]=60 [bebopd]=30 [fugitoidd]=60 [overlordd]=30 [nulld]=30
)

# ── Simple + technical explanations (richer prose kept from the live version) ──
declare -A DAEMON_SIMPLE=(
  [burned]="Your phone has tracking code baked into it at factory level by Xiaomi. This isn't something you installed — it's built into the firmware. Like buying a car and finding the dealer already fitted a GPS tracker you didn't ask for."
  [granitord]="Checks the security foundations of your device — whether the locks are on and nobody has tampered with the core OS. A low score means those foundations are weaker than they should be."
  [leatherheadd]="Your phone is generating heat. When it gets too hot, it slows processor cores to avoid damage — like a car engine cutting power to prevent seizure. A critical score means it needs to cool down."
  [metalheadd]="Watches your phone's sensors — accelerometer, gyroscope, microphone access. Apps shouldn't poll movement sensors in the background. If they are, something may be tracking you without your knowledge."
  [rahzerd]="Monitors all your network connections — WiFi, mobile data, DNS, open connections. Think of it as watching every door and window of your house to see what's coming in and going out."
  [ratkingd]="Watches your phone's memory and processes. Zombie processes are programs that crashed but haven't shut down — like a ghost still haunting the house. High counts make your phone unstable."
  [rocksteadyd]="Monitors how hard your processor is working. When cores throttle down your phone slows noticeably. Sustained throttling means something is pushing the hardware too hard in the background."
  [shredderd]="Checks whether the kernel — the deepest part of the OS — has been modified. Like checking whether a building's foundations have been tampered with. Unexpected modules could indicate a rootkit."
  [tigerclawd]="The eye inside HyperOS — learns what your device normally looks like and flags anything that deviates. Like a security guard who knows every staff member and immediately spots a stranger."
  [bebopd]="Measures how fast your battery is draining. A healthy phone at rest should barely use power. Fast drain when idle means something is running in the background — apps, telemetry, or worse."
  [fugitoidd]="Reads your system crash logs. App crashes, ANRs (App Not Responding freezes) and out-of-memory kills all leave traces. Frequent crashes mean your phone is running too hard or has a misbehaving app."
  [overlordd]="Cross-daemon correlation engine — connects signals from every other daemon to spot patterns none of them would catch alone. Three unrelated warnings firing together is worth more attention than any one alone."
  [nulld]="Watches what runs on your phone while the screen is off — flags apps quietly sending data during idle, when nothing should be talking to the outside world."
)

declare -A DAEMON_TECHNICAL=(
  [burned]="Partner/advertiser IDs (Facebook, AppsFlyer) embedded at ROM level. Cannot be removed without flashing. Verifies persist.sys.miui_optimization and GDPR opt-out flags on EEA build."
  [granitord]="Kernel security posture: SELinux state, verified boot chain, ro.secure/ro.debuggable props, dm-verity status. Score across 7 integrity vectors."
  [leatherheadd]="MTK thermal zone polling via HAL thermalservice. Throttling via scaling_cur_freq deltas. Score <60 indicates sustained thermal stress across zones."
  [metalheadd]="Sensor access via /proc/[pid]/fd scanning for sensor device nodes. Sensitive classification: ACCELEROMETER, GYROSCOPE, MAGNETIC_FIELD. Background access flagged without foreground activity."
  [rahzerd]="Pre-baked connectivity audit via adb_cli. Parses telephony.registry dumpsys for RAT/registration. DNS via getaddrinfo(). TCP count from /proc/net/tcp. Baseline drift via BSSID/DNS latency comparison."
  [ratkingd]="Parses /proc/[pid]/status for zombie state (Z). Orphan detection via PPID=1 scan. Memory pressure from /proc/meminfo MemAvailable. OOM kills via logcat Low on memory/Kill entries."
  [rocksteadyd]="Per-core frequency via scaling_cur_freq. Throttle detected when freq drops below 60% of max_freq for >3 consecutive polls. Score by core count and throttle depth."
  [shredderd]="/proc/modules inventory vs baseline DJB2 hash. Flags new modules not present at baseline. Rootkit sigs: su, hide, xposed, magisk, riru, zygisk, lsposed. Score = 100-(new*20 + rootkit*30 + sysfs*10)."
  [tigerclawd]="Binder service registry topology via adb_cli. Baseline fingerprints 473 services at first run. DJB2 hash of ro.* props for drift detection. Trust score across 6 integrity vectors."
  [bebopd]="Drain via /sys/class/power_supply/battery/charge_now delta over poll interval. mAh/hr from current_now. Drain >400mAh/hr at idle is anomalous."
  [fugitoidd]="Logcat parsing for FATAL EXCEPTION, ANR in, Low on memory/Kill entries. OOM kill events from kernel log. Timestamps correlated against process table for attribution."
  [overlordd]="Correlation: thermal+CPU+battery = hardware stress. network+binder+memory = anomaly. privacy+kernel+trust = compromise. Scoring: 1-daemon=1, 2-daemon=3, 3+=5."
  [nulld]="Polls screen state every 10s. On screen-off, baselines TCP connections + per-UID netstats, then watches for spikes during idle. Correlates transmitting UIDs against expected idle behavior."
)

_ts() { date '+%H:%M:%S'; }

# ── Config load, with malformed-JSON repair ────────────────────────────────────
load_config() {
    if [ -f "$CONFIG" ]; then
        local raw
        raw=$(cat "$CONFIG" 2>/dev/null)
        if ! echo "$raw" | jq empty 2>/dev/null; then
            raw="${raw}}"
            echo "$raw" | jq empty 2>/dev/null || raw=""
        fi
        if [ -n "$raw" ]; then
            ENABLED=$(echo "$raw" | jq -r '.enabled_daemons[]?' 2>/dev/null | tr '\n' ' ')
        fi
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

# ── Consent gate ────────────────────────────────────────────────────────────────
consent_gate() {
    [ -f "$CONSENT_LOCK" ] && return 0
    clear
    printf "${BOLD}${WHT}"
    printf "╔══════════════════════════════════════════════════════════════════════╗\n"
    printf "║     MIUISERPERUSER — SYNDICATE SCAN MODE                             ║\n"
    printf "║          Full Device Intelligence Scan                               ║\n"
    printf "╚══════════════════════════════════════════════════════════════════════╝\n"
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
    printf "══════════════════════════════════════════════════════════════════════\n\n"
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

pre_scan_check() {
    local available=0
    local missing=0
    for d in $ENABLED; do
        [ -x "$BIN/$d" ] && available=$((available+1)) || missing=$((missing+1))
    done

    [ $missing -gt 0 ] && {
        printf "\n  ${ORN}⚠ WARNING: %d of %d enabled daemons have no binary${RST}\n" "$missing" "$((available+missing))"
        printf "  ${DIM}Only %d daemons will scan. Build missing binaries in:%s${RST}\n\n" "$available" "$BIN"
        printf "  ${DIM}Press any key to continue...${RST}"
        read -n1
        printf "\n\n"
    }
}

# ── Per-daemon extra env (fixes the $extra_env word-splitting bug — use `env`) ──
_daemon_env() {
    case "$1" in
        granitord) echo "GRANITORD_POLL_SEC=3" ;;
        fugitoidd) echo "BEXEC_NO_RISH=1" ;;
        *) echo "" ;;
    esac
}

_launch_daemon() {
    local d="$1" bin="$2" to="$3"
    local extra
    extra=$(_daemon_env "$d")
    (
        export RISH_APPLICATION_ID=com.termux
        env $extra timeout "$to" "$bin" >> "$LOGS/${d}.log" 2>&1
    ) &
    echo $!
}

# ── Ordered daemon list — granitord first (slowest daemon, wants a head start) ──
_ordered_enabled() {
    local ordered=""
    for d in $ENABLED; do
        [ "$d" = "granitord" ] && ordered="granitord $ordered"
    done
    for d in $ENABLED; do
        [ "$d" != "granitord" ] && ordered="$ordered $d"
    done
    echo "$ordered"
}

_parse_and_print() {
    local d="$1" f="$2" name="$3"
    if [ ! -f "$f" ]; then
        printf "  ${RED}[%s] [FAIL] [%s] No JSON output${RST}\n" "$(_ts)" "$name"
        return
    fi
    case "$d" in
        burned)
            local sigs=$(jq -r '.privacy_signal_count // 0' "$f" 2>/dev/null)
            [ "$sigs" -gt 0 ] 2>/dev/null && \
                printf "  ${YEL}[%s] [WARN] [%s] %s privacy signals${RST}\n" "$(_ts)" "$name" "$sigs" || \
                printf "  ${GRN}[%s] [OK]   [%s] No privacy signals${RST}\n" "$(_ts)" "$name"
            ;;
        granitord)
            local score=$(jq -r '.posture.score // "?"' "$f" 2>/dev/null)
            local grade=$(jq -r '.posture.grade // "?"' "$f" 2>/dev/null)
            printf "  ${GRN}[%s] [INFO] [%s] Posture: %s/100 — %s${RST}\n" "$(_ts)" "$name" "$score" "$grade"
            ;;
        leatherheadd)
            local score=$(jq -r '.thermal_score // "?"' "$f" 2>/dev/null)
            local throttled=$(jq -r '.throttled_cores // 0' "$f" 2>/dev/null)
            [ "$throttled" -gt 0 ] 2>/dev/null && \
                printf "  ${YEL}[%s] [WARN] [%s] %s cores throttled, score %s${RST}\n" "$(_ts)" "$name" "$throttled" "$score" || \
                printf "  ${GRN}[%s] [OK]   [%s] Score %s, no throttling${RST}\n" "$(_ts)" "$name" "$score"
            ;;
        metalheadd)
            local active=$(jq -r '.sensitive_active // 0' "$f" 2>/dev/null)
            [ "$active" -gt 0 ] 2>/dev/null && \
                printf "  ${YEL}[%s] [WARN] [%s] %s sensors active in background${RST}\n" "$(_ts)" "$name" "$active" || \
                printf "  ${GRN}[%s] [OK]   [%s] No suspicious sensors${RST}\n" "$(_ts)" "$name"
            ;;
        rahzerd)
            local tcp4=$(jq -r '.ports.established_tcp4 // 0' "$f" 2>/dev/null)
            local tcp6=$(jq -r '.ports.established_tcp6 // 0' "$f" 2>/dev/null)
            local dns=$(jq -r '.dns.latency_ms // "?"' "$f" 2>/dev/null)
            printf "  ${GRN}[%s] [INFO] [%s] %s TCP4 + %s TCP6, DNS %sms${RST}\n" "$(_ts)" "$name" "$tcp4" "$tcp6" "$dns"
            ;;
        ratkingd)
            local zombies=$(jq -r '.processes.zombies // 0' "$f" 2>/dev/null)
            local avail=$(jq -r '.memory.available_mb // .pressure.avail_mb // "?"' "$f" 2>/dev/null)
            [ "$zombies" -gt 0 ] 2>/dev/null && \
                printf "  ${YEL}[%s] [WARN] [%s] %s zombies, %s MB avail${RST}\n" "$(_ts)" "$name" "$zombies" "$avail" || \
                printf "  ${GRN}[%s] [OK]   [%s] %s MB avail, no zombies${RST}\n" "$(_ts)" "$name" "$avail"
            ;;
        rocksteadyd)
            local score=$(jq -r '.cpu_score // "?"' "$f" 2>/dev/null)
            local throttled=$(jq -r '.throttled_cores // 0' "$f" 2>/dev/null)
            [ "$throttled" -gt 0 ] 2>/dev/null && \
                printf "  ${YEL}[%s] [WARN] [%s] %s cores throttled, score %s${RST}\n" "$(_ts)" "$name" "$throttled" "$score" || \
                printf "  ${GRN}[%s] [OK]   [%s] Score %s, all nominal${RST}\n" "$(_ts)" "$name" "$score"
            ;;
        shredderd)
            local score grade modules newmod
            IFS=$'\t' read -r score grade modules newmod < <(jq -r \
                '[(.integrity.score // "?"), (.integrity.grade // "?"), (.baseline.module_count // "?"), ((.drift.new_modules // []) | length)] | @tsv' \
                "$f" 2>/dev/null)
            [ "$newmod" -gt 0 ] 2>/dev/null && \
                printf "  ${RED}[%s] [ALERT] [%s] %s new modules! Score %s${RST}\n" "$(_ts)" "$name" "$newmod" "$score" || \
                printf "  ${GRN}[%s] [OK]   [%s] Score %s, no new modules${RST}\n" "$(_ts)" "$name" "$score"
            ;;
        tigerclawd)
            local trust=$(jq -r '.trust_score // "?"' "$f" 2>/dev/null)
            local drift=$(jq -r '.binder.drift // 0' "$f" 2>/dev/null)
            [ "$drift" -gt 0 ] 2>/dev/null && \
                printf "  ${YEL}[%s] [WARN] [%s] %s drifted, trust %s/100${RST}\n" "$(_ts)" "$name" "$drift" "$trust" || \
                printf "  ${GRN}[%s] [OK]   [%s] Trust %s/100, no drift${RST}\n" "$(_ts)" "$name" "$trust"
            ;;
        bebopd)
            local drain=$(jq -r '.drain_mah_h // .drain_mah_per_hour // "?"' "$f" 2>/dev/null)
            local health=$(jq -r '.grade // "?"' "$f" 2>/dev/null)
            case "$health" in
                F|D) printf "  ${RED}[%s] [ALERT] [%s] %s mAh/hr — Grade %s${RST}\n" "$(_ts)" "$name" "$drain" "$health" ;;
                C)   printf "  ${YEL}[%s] [WARN] [%s] %s mAh/hr — Grade %s${RST}\n" "$(_ts)" "$name" "$drain" "$health" ;;
                *)   printf "  ${GRN}[%s] [OK]   [%s] %s mAh/hr — Grade %s${RST}\n" "$(_ts)" "$name" "$drain" "$health" ;;
            esac
            ;;
        fugitoidd)
            local crashes=$(jq -r '.crashes // .crash_count // 0' "$f" 2>/dev/null)
            local anrs=$(jq -r '.anrs // .anr_count // 0' "$f" 2>/dev/null)
            local ooms=$(jq -r '.oom_events // .oom_count // 0' "$f" 2>/dev/null)
            local total_events=$((crashes + anrs + ooms))
            [ "$total_events" -gt 0 ] 2>/dev/null && \
                printf "  ${YEL}[%s] [WARN] [%s] %s crashes, %s ANRs, %s OOMs${RST}\n" "$(_ts)" "$name" "$crashes" "$anrs" "$ooms" || \
                printf "  ${GRN}[%s] [OK]   [%s] No crash events${RST}\n" "$(_ts)" "$name"
            ;;
        overlordd)
            local sigs=$(jq -r '.correlation.anomaly_count // 0' "$f" 2>/dev/null)
            local status=$(jq -r '.correlation.status // "?"' "$f" 2>/dev/null)
            case "$status" in
                COMPROMISED|SUSPICIOUS) \
                    printf "  ${RED}[%s] [ALERT] [%s] %s — %s anomalies${RST}\n" "$(_ts)" "$name" "$status" "$sigs" ;;
                *) \
                    printf "  ${GRN}[%s] [OK]   [%s] %s — %s anomalies${RST}\n" "$(_ts)" "$name" "$status" "$sigs" ;;
            esac
            ;;
        nulld)
            local screen spike events
            IFS=$'\t' read -r screen spike events < <(jq -r \
                '[.screen, (.idle_spike_detected|tostring), (.total_spike_events // 0)] | @tsv' \
                "$f" 2>/dev/null)
            [ "$spike" = "true" ] && \
                printf "  ${RED}[%s] [ALERT] [%s] Idle spike detected! (%s total events, screen=%s)${RST}\n" "$(_ts)" "$name" "$events" "$screen" || \
                printf "  ${GRN}[%s] [OK]   [%s] No idle spikes (screen=%s)${RST}\n" "$(_ts)" "$name" "$screen"
            ;;
        *)
            printf "  ${GRN}[%s] [INFO] [%s] Scan complete${RST}\n" "$(_ts)" "$name"
            ;;
    esac
}

# ═══════════════════════════════════════════════════════════════════════════════
# ROLLING SCAN — parallel launch (granitord gets a head start), live feed,
# pre-bakes rahzerd via dump_rahzerd.sh if present (kept from the live version)
# ═══════════════════════════════════════════════════════════════════════════════
rolling_scan() {
    local round=1
    local available_only=${1:-0}

    clear
    printf "${BOLD}${WHT}"
    printf "╔══════════════════════════════════════════════════════════════════════╗\n"
    printf "║  SYNDICATE ROLLING SCAN (parallel)                                   ║\n"
    printf "║  Live feed — Ctrl+C to stop this round                               ║\n"
    printf "╚══════════════════════════════════════════════════════════════════════╝\n"
    printf "${RST}\n"

    pre_scan_check

    local stop=0
    trap 'stop=1' INT

    while [ $stop -eq 0 ]; do
        printf "  ${MAG}[%s] [INFO] Starting round %d (parallel)...${RST}\n\n" "$(_ts)" "$round"

        # Pre-bake rahzerd's connectivity dump if the helper script exists
        [ -x "$SCRIPTS/dump_rahzerd.sh" ] && "$SCRIPTS/dump_rahzerd.sh" 2>/dev/null

        local ds=() pids=() fs=() names=()
        for d in $(_ordered_enabled); do
            local bin="$BIN/$d"
            if [ ! -x "$bin" ]; then
                [ "$available_only" -eq 0 ] && \
                    printf "  ${ORN}[%s] [SKIP] [%s] Binary not found${RST}\n" "$(_ts)" "${DAEMON_NAME[$d]:-$d}"
                continue
            fi
            local f="$RESULTS/${d}.json"
            rm -f "$f"
            local to=${DAEMON_TIMEOUT[$d]:-30}
            local pid=$(_launch_daemon "$d" "$bin" "$to")
            ds+=("$d"); pids+=("$pid"); fs+=("$f"); names+=("${DAEMON_NAME[$d]:-$d}")
            printf "  ${CYN}[%s] [SCAN] [%s] Launched (timeout: %ds)...${RST}\n" "$(_ts)" "${DAEMON_NAME[$d]:-$d}" "$to"
            # Small head start for granitord before firing the rest — it's the
            # slowest daemon (120s timeout) and benefits from an early launch.
            [ "$d" = "granitord" ] && sleep 1
        done

        local total=${#pids[@]}
        [ $total -eq 0 ] && {
            printf "\n  ${RED}[%s] [ERROR] No daemon binaries found in %s${RST}\n" "$(_ts)" "$BIN"
            printf "  ${DIM}Build your daemons or check the bin/ directory.${RST}\n\n"
            sleep 3
            trap - INT
            return
        }

        printf "\n  ${DIM}Waiting on %d daemons in parallel...${RST}\n\n" "$total"

        local done_flags=()
        for ((i=0; i<total; i++)); do done_flags[i]=0; done
        local remaining=$total

        while [ $remaining -gt 0 ] && [ $stop -eq 0 ]; do
            for ((i=0; i<total; i++)); do
                [ "${done_flags[i]}" -eq 1 ] && continue
                if ! kill -0 "${pids[i]}" 2>/dev/null; then
                    wait "${pids[i]}" 2>/dev/null
                    local ec=$?
                    printf "  ${GRN}[%s] [DONE] [%s] exit: %d${RST}\n" "$(_ts)" "${names[i]}" "$ec"
                    _parse_and_print "${ds[i]}" "${fs[i]}" "${names[i]}"
                    done_flags[i]=1
                    remaining=$((remaining-1))
                fi
            done
            [ $remaining -gt 0 ] && sleep 0.3
        done

        if [ $stop -eq 1 ]; then
            printf "\n\n  ${GRN}[%s] [STOP] Rolling scan halted (daemons still running in background will finish)${RST}\n\n" "$(_ts)"
            trap - INT
            return
        fi

        printf "\n  ${MAG}[%s] [INFO] Round %d complete. Next in 2s...${RST}\n\n" "$(_ts)" "$round"
        sleep 2
        round=$((round+1))
    done

    trap - INT
}

# ═══════════════════════════════════════════════════════════════════════════════
# PAGED RESULTS — per-daemon prev/next viewer with the richer WHAT-THIS-MEANS /
# TECHNICAL prose kept from the live version's inline explanations
# ═══════════════════════════════════════════════════════════════════════════════
show_paged_results() {
    local daemon_list=()
    for d in $ENABLED; do
        daemon_list+=("$d")
    done

    local total=${#daemon_list[@]}
    [ $total -eq 0 ] && {
        clear
        printf "\n${RED} NO DAEMONS ENABLED ${RST}\n\n"
        printf "  ${DIM}Press any key...${RST}"
        read -n1
        return
    }

    local idx=0
    local choice=""

    while true; do
        local d="${daemon_list[$idx]}"
        local f="$RESULTS/${d}.json"
        local name="${DAEMON_NAME[$d]:-$d}"
        local simple="${DAEMON_SIMPLE[$d]}"
        local technical="${DAEMON_TECHNICAL[$d]}"
        local has_binary="no"
        local has_json="no"
        [ -x "$BIN/$d" ] && has_binary="yes"
        [ -f "$f" ] && has_json="yes"

        clear
        printf "${BOLD}${WHT}"
        printf "╔══════════════════════════════════════════════════════════════════════╗\n"
        printf "║  %-66s  ║\n" "SYNDICATE RESULTS — ${name}"
        printf "║  %-66s  ║\n" "Daemon $((idx+1)) of $total"
        printf "╚══════════════════════════════════════════════════════════════════════╝\n"
        printf "${RST}\n"

        if [ "$has_binary" = "no" ]; then
            printf "  ${RED}● BINARY MISSING${RST}  ${DIM}%s${RST}\n\n" "$BIN/$d"
        elif [ "$has_json" = "no" ]; then
            printf "  ${ORN}● NOT YET SCANNED${RST}  ${DIM}Run rolling scan to populate${RST}\n\n"
        else
            printf "  ${GRN}● DATA AVAILABLE${RST}  ${DIM}%s${RST}\n\n" "$f"
        fi

        printf "${BOLD}${BLU}"
        printf "┌────────────────────────────────────────────────────────────────────┐\n"
        printf "│  %-64s│\n" "RAW JSON FINDINGS"
        printf "├────────────────────────────────────────────────────────────────────┤\n"
        printf "${RST}"

        if [ "$has_json" = "yes" ]; then
            case "$d" in
                burned)
                    local sigs=$(jq -r '.privacy_signal_count // 0' "$f" 2>/dev/null)
                    local list=$(jq -r '.privacy_list // ""' "$f" 2>/dev/null | tr ',' '\n' | sort -u | tr '\n' ' ')
                    printf "${BLU}│${RST}  ${WHT}Privacy Signals:${RST} ${YEL}%s${RST}\n" "$sigs"
                    printf "${BLU}│${RST}\n"
                    while IFS= read -r tracker; do
                        [ -n "$tracker" ] && printf "${BLU}│${RST}    ${DIM}•${RST} ${YEL}%s${RST}\n" "$tracker"
                    done <<< "$(echo "$list" | tr ' ' '\n')"
                    ;;
                granitord)
                    local score=$(jq -r '.posture.score // "?"' "$f" 2>/dev/null)
                    local grade=$(jq -r '.posture.grade // "?"' "$f" 2>/dev/null)
                    local selinux=$(jq -r '.posture.selinux // "?"' "$f" 2>/dev/null)
                    local boot=$(jq -r '.posture.verified_boot // "?"' "$f" 2>/dev/null)
                    printf "${BLU}│${RST}  ${WHT}Score:${RST} ${YEL}%s/100${RST}  ${WHT}Grade:${RST} ${YEL}%s${RST}\n" "$score" "$grade"
                    printf "${BLU}│${RST}  ${WHT}SELinux:${RST} ${YEL}%s${RST}  ${WHT}Boot:${RST} ${YEL}%s${RST}\n" "$selinux" "$boot"
                    ;;
                leatherheadd)
                    local score=$(jq -r '.thermal_score // "?"' "$f" 2>/dev/null)
                    local grade=$(jq -r '.grade // "?"' "$f" 2>/dev/null)
                    local throttled=$(jq -r '.throttled_cores // 0' "$f" 2>/dev/null)
                    local maxt=$(jq -r '.max_temp_c // "?"' "$f" 2>/dev/null)
                    printf "${BLU}│${RST}  ${WHT}Thermal Score:${RST} ${YEL}%s/100${RST}  ${WHT}Grade:${RST} ${YEL}%s${RST}\n" "$score" "$grade"
                    printf "${BLU}│${RST}  ${WHT}Throttled Cores:${RST} ${YEL}%s${RST}  ${WHT}Max Temp:${RST} ${YEL}%s°C${RST}\n" "$throttled" "$maxt"
                    ;;
                metalheadd)
                    local score=$(jq -r '.sensor_score // "?"' "$f" 2>/dev/null)
                    local active=$(jq -r '.sensitive_active // 0' "$f" 2>/dev/null)
                    local bg=$(jq -r '.background_pollers // 0' "$f" 2>/dev/null)
                    printf "${BLU}│${RST}  ${WHT}Sensor Score:${RST} ${YEL}%s/100${RST}\n" "$score"
                    printf "${BLU}│${RST}  ${WHT}Sensitive Active:${RST} ${YEL}%s${RST}  ${WHT}Background:${RST} ${YEL}%s${RST}\n" "$active" "$bg"
                    ;;
                rahzerd)
                    local wifi=$(jq -r '.wifi.connected // -1' "$f" 2>/dev/null)
                    local rat=$(jq -r '.mobile.rat // "?"' "$f" 2>/dev/null)
                    local dns=$(jq -r '.dns.latency_ms // "?"' "$f" 2>/dev/null)
                    local tcp4=$(jq -r '.ports.established_tcp4 // 0' "$f" 2>/dev/null)
                    local tcp6=$(jq -r '.ports.established_tcp6 // 0' "$f" 2>/dev/null)
                    printf "${BLU}│${RST}  ${WHT}WiFi:${RST} ${YEL}%s${RST}  ${WHT}Mobile:${RST} ${YEL}%s${RST}  ${WHT}DNS:${RST} ${YEL}%sms${RST}\n" \
                        "$([ "$wifi" = "1" ] && echo "connected" || echo "disconnected")" "$rat" "$dns"
                    printf "${BLU}│${RST}  ${WHT}Connections:${RST} ${YEL}%s TCP4 + %s TCP6${RST}\n" "$tcp4" "$tcp6"
                    ;;
                ratkingd)
                    local zombies=$(jq -r '.processes.zombies // 0' "$f" 2>/dev/null)
                    local orphans=$(jq -r '.processes.orphans // 0' "$f" 2>/dev/null)
                    local avail=$(jq -r '.memory.available_mb // .pressure.avail_mb // "?"' "$f" 2>/dev/null)
                    local totalm=$(jq -r '.memory.total_mb // "?"' "$f" 2>/dev/null)
                    printf "${BLU}│${RST}  ${WHT}Memory:${RST} ${YEL}%s${RST}/${DIM}%s${RST} ${WHT}MB available${RST}\n" "$avail" "$totalm"
                    printf "${BLU}│${RST}  ${WHT}Zombies:${RST} ${YEL}%s${RST}  ${WHT}Orphans:${RST} ${YEL}%s${RST}\n" "$zombies" "$orphans"
                    ;;
                rocksteadyd)
                    local score=$(jq -r '.cpu_score // "?"' "$f" 2>/dev/null)
                    local throttled=$(jq -r '.throttled_cores // 0' "$f" 2>/dev/null)
                    local gov=$(jq -r '.governor // "?"' "$f" 2>/dev/null)
                    printf "${BLU}│${RST}  ${WHT}CPU Score:${RST} ${YEL}%s/100${RST}\n" "$score"
                    printf "${BLU}│${RST}  ${WHT}Throttled:${RST} ${YEL}%s cores${RST}  ${WHT}Governor:${RST} ${YEL}%s${RST}\n" "$throttled" "$gov"
                    ;;
                shredderd)
                    local score grade modules newmod
                    IFS=$'\t' read -r score grade modules newmod < <(jq -r \
                        '[(.integrity.score // "?"), (.integrity.grade // "?"), (.baseline.module_count // "?"), ((.drift.new_modules // []) | length)] | @tsv' \
                        "$f" 2>/dev/null)
                    printf "${BLU}│${RST}  ${WHT}Integrity:${RST} ${YEL}%s/100${RST}  ${WHT}Grade:${RST} ${YEL}%s${RST}\n" "$score" "$grade"
                    printf "${BLU}│${RST}  ${WHT}Modules:${RST} ${YEL}%s${RST}  ${WHT}New:${RST} ${YEL}%s${RST}\n" "$modules" "$newmod"
                    ;;
                tigerclawd)
                    local trust=$(jq -r '.trust_score // "?"' "$f" 2>/dev/null)
                    local svcs=$(jq -r '.binder.service_count // "?"' "$f" 2>/dev/null)
                    local drift=$(jq -r '.binder.drift // 0' "$f" 2>/dev/null)
                    local ver=$(jq -r '.device.hyperos_version // "?"' "$f" 2>/dev/null)
                    local patch=$(jq -r '.device.security_patch // "?"' "$f" 2>/dev/null)
                    printf "${BLU}│${RST}  ${WHT}Trust:${RST} ${YEL}%s/100${RST}  ${WHT}HyperOS:${RST} ${YEL}%s${RST}\n" "$trust" "$ver"
                    printf "${BLU}│${RST}  ${WHT}Services:${RST} ${YEL}%s${RST}  ${WHT}Drift:${RST} ${YEL}%s${RST}  ${WHT}Patch:${RST} ${YEL}%s${RST}\n" "$svcs" "$drift" "$patch"
                    ;;
                bebopd)
                    local drain=$(jq -r '.drain_mah_h // .drain_mah_per_hour // "?"' "$f" 2>/dev/null)
                    local health=$(jq -r '.grade // "?"' "$f" 2>/dev/null)
                    local curr=$(jq -r '.current_ma // "?"' "$f" 2>/dev/null)
                    printf "${BLU}│${RST}  ${WHT}Drain:${RST} ${YEL}%s mAh/hr${RST}  ${WHT}Grade:${RST} ${YEL}%s${RST}\n" "$drain" "$health"
                    printf "${BLU}│${RST}  ${WHT}Current:${RST} ${YEL}%s mA${RST}\n" "$curr"
                    ;;
                fugitoidd)
                    local crashes=$(jq -r '.crashes // .crash_count // 0' "$f" 2>/dev/null)
                    local anrs=$(jq -r '.anrs // .anr_count // 0' "$f" 2>/dev/null)
                    local ooms=$(jq -r '.oom_events // .oom_count // 0' "$f" 2>/dev/null)
                    local last=$(jq -r '.last_crash // "?"' "$f" 2>/dev/null)
                    printf "${BLU}│${RST}  ${WHT}Crashes:${RST} ${YEL}%s${RST}  ${WHT}ANRs:${RST} ${YEL}%s${RST}  ${WHT}OOMs:${RST} ${YEL}%s${RST}\n" "$crashes" "$anrs" "$ooms"
                    printf "${BLU}│${RST}  ${WHT}Last Event:${RST} ${DIM}%s${RST}\n" "$last"
                    ;;
                overlordd)
                    local sigs=$(jq -r '.correlation.anomaly_count // 0' "$f" 2>/dev/null)
                    local status=$(jq -r '.correlation.status // "?"' "$f" 2>/dev/null)
                    local hw=$(jq -r '.correlation.hardware_stress // "?"' "$f" 2>/dev/null)
                    printf "${BLU}│${RST}  ${WHT}Anomalies:${RST} ${YEL}%s${RST}  ${WHT}Status:${RST} ${YEL}%s${RST}\n" "$sigs" "$status"
                    printf "${BLU}│${RST}  ${WHT}Hardware Stress:${RST} ${YEL}%s${RST}\n" "$hw"
                    ;;
                nulld)
                    local screen idle_s tcp4 tcp6 spike events susp
                    IFS=$'\t' read -r screen idle_s tcp4 tcp6 spike events susp < <(jq -r \
                        '[.screen, .idle_seconds, .connections.tcp4, .connections.tcp6, (.idle_spike_detected|tostring), .total_spike_events, .suspicious_transmitters] | @tsv' \
                        "$f" 2>/dev/null)
                    printf "${BLU}│${RST}  ${WHT}Screen:${RST} ${YEL}%s${RST}  ${WHT}Idle:${RST} ${YEL}%ss${RST}\n" "$screen" "$idle_s"
                    printf "${BLU}│${RST}  ${WHT}TCP:${RST} ${YEL}%s/%s${RST}  ${WHT}Spike:${RST} ${YEL}%s${RST}  ${WHT}Events:${RST} ${YEL}%s${RST}\n" "$tcp4" "$tcp6" "$spike" "$events"
                    [ -n "$susp" ] && printf "${BLU}│${RST}  ${WHT}Suspicious:${RST} ${DIM}%s${RST}\n" "$susp"
                    ;;
                *)
                    printf "${BLU}│${RST}  ${DIM}(No parser for this daemon)${RST}\n"
                    ;;
            esac
        else
            printf "${BLU}│${RST}\n"
            if [ "$has_binary" = "no" ]; then
                printf "${BLU}│${RST}  ${RED}Binary not found:${RST} ${DIM}$BIN/$d${RST}\n"
                printf "${BLU}│${RST}  ${DIM}Build this daemon to see results here.${RST}\n"
            else
                printf "${BLU}│${RST}  ${ORN}No scan data yet.${RST}\n"
                printf "${BLU}│${RST}  ${DIM}Run 'Rolling Scan' to populate this daemon's results.${RST}\n"
            fi
            printf "${BLU}│${RST}\n"
        fi

        printf "${BOLD}${BLU}"
        printf "└────────────────────────────────────────────────────────────────────┘\n"
        printf "${RST}\n"

        printf "  ${BOLD}${WHT}WHAT THIS MEANS:${RST}\n"
        printf "  ${WHT}%s${RST}\n\n" "$simple"

        printf "  ${BOLD}${CYN}TECHNICAL:${RST}\n"
        printf "  ${DIM}%s${RST}\n\n" "$technical"

        printf "${CYN}══════════════════════════════════════════════════════════════════════${RST}\n"
        printf "  ${WHT}[${GRN}p${WHT}]${RST} Previous  ${WHT}[${GRN}n${WHT}]${RST} Next  ${WHT}[${GRN}r${WHT}]${RST} Re-scan  ${WHT}[${GRN}q${WHT}]${RST} Quit to menu\n"
        printf "${CYN}══════════════════════════════════════════════════════════════════════${RST}\n"
        printf "  ${CYN}Choice: ${RST}"

        read -n1 -r choice
        printf "\n"

        case "$choice" in
            p|P)
                idx=$((idx-1))
                [ $idx -lt 0 ] && idx=$((total-1))
                ;;
            n|N)
                idx=$((idx+1))
                [ $idx -ge $total ] && idx=0
                ;;
            r|R)
                if [ ! -x "$BIN/$d" ]; then
                    clear
                    printf "\n  ${RED}Cannot scan — binary missing${RST}\n\n"
                    sleep 2
                else
                    clear
                    printf "\n  ${MAG}Re-scanning ${name}...${RST}\n"
                    rm -f "$f"
                    local to=${DAEMON_TIMEOUT[$d]:-30}
                    local pid=$(_launch_daemon "$d" "$BIN/$d" "$to")
                    wait "$pid" 2>/dev/null
                    printf "  ${GRN}✔ Done${RST}\n"
                    sleep 1
                fi
                ;;
            q|Q)
                break
                ;;
        esac
    done
}

# ═══════════════════════════════════════════════════════════════════════════════
# FAST SCAN — Parallel batch, one-shot summary (formerly "run_scan" in the live
# version — kept the meaty inline summary block, now bexec-fixed and parallel)
# ═══════════════════════════════════════════════════════════════════════════════
run_fast_scan() {
    local round=$1
    clear
    printf "\n${BOLD}${WHT}  FAST SCAN — ROUND ${round}${RST}\n"
    printf "  ${DIM}$(date '+%Y-%m-%d %H:%M:%S')${RST}\n\n"
    printf "  ${MAG}Firing all daemons in parallel...${RST}\n\n"

    rm -f "$RESULTS"/*.json

    [ -x "$SCRIPTS/dump_rahzerd.sh" ] && "$SCRIPTS/dump_rahzerd.sh" 2>/dev/null

    local pids=()
    local started=$(date +%s)

    for d in $(_ordered_enabled); do
        local bin="$BIN/$d"
        [ ! -x "$bin" ] && continue
        local to=${DAEMON_TIMEOUT[$d]:-30}
        local pid=$(_launch_daemon "$d" "$bin" "$to")
        pids+=("$pid")
        printf "  ${CYN}%-14s${RST} ${DIM}launched (timeout ${to}s)${RST}\n" "$d"
        [ "$d" = "granitord" ] && sleep 1
    done

    local total=${#pids[@]}
    local done_count=0
    local spin='⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏'
    local spin_idx=0

    printf "\n  ${DIM}Waiting for ${total} daemons...${RST}\n"

    while [ $done_count -lt $total ]; do
        done_count=0
        for pid in "${pids[@]}"; do
            if ! kill -0 "$pid" 2>/dev/null; then
                done_count=$((done_count+1))
            fi
        done
        local char="${spin:$((spin_idx % 10)):1}"
        spin_idx=$((spin_idx+1))
        local elapsed=$(($(date +%s)-started))
        printf "\r  ${CYN}${char}${RST} ${done_count}/${total} complete ${DIM}(${elapsed}s)${RST}  "
        sleep 0.2
    done

    for pid in "${pids[@]}"; do wait "$pid" 2>/dev/null; done

    local elapsed=$(($(date +%s)-started))
    printf "\r  ${GRN}✔${RST} All ${total} daemons finished in ${elapsed}s${DIM}                    ${RST}\n\n"

    # Meaty inline summary, per daemon, using the richer prose above
    printf "  ${BOLD}${WHT}SCAN RESULTS:${RST}\n"
    printf "${CYN}%s${RST}\n" "════════════════════════════════════════════"
    for d in $ENABLED; do
        local f="$RESULTS/${d}.json"
        local name="${DAEMON_NAME[$d]:-$d}"
        if [ -f "$f" ]; then
            local ts=$(jq -r '.timestamp // "?"' "$f" 2>/dev/null | grep -oE '[0-9]{2}:[0-9]{2}' | head -1)
            printf "\n  ${GRN}✔${RST} ${BOLD}${CYN}%s${RST} ${DIM}@ %s${RST}\n" "$name" "$ts"
            _parse_and_print "$d" "$f" "$name"
            printf "\n    ${BOLD}${WHT}WHAT THIS MEANS:${RST}\n    ${WHT}%s${RST}\n" "${DAEMON_SIMPLE[$d]}"
            printf "\n    ${BOLD}${CYN}TECHNICAL:${RST}\n    ${DIM}%s${RST}\n" "${DAEMON_TECHNICAL[$d]}"
            printf "${CYN}%s${RST}\n" "------------------------------------------"
        else
            printf "\n  ${DIM}○ %-14s no results${RST}\n" "$name"
        fi
    done
    printf "\n${CYN}%s${RST}\n" "════════════════════════════════════════════"

    local result_count=0
    for d in $ENABLED; do
        [ -f "$RESULTS/${d}.json" ] && result_count=$((result_count+1))
    done
    printf "\n  ${GRN}✔${RST} ${result_count}/${total} daemons produced results\n\n"
}

# ═══════════════════════════════════════════════════════════════════════════════
# LIVE VIEW — cursor-repositioning dashboard, kept from the live version
# ═══════════════════════════════════════════════════════════════════════════════
live_view() {
    clear
    printf "${BOLD}${WHT}  SYNDICATE LIVE VIEW${RST}  ${DIM}Ctrl+C to exit${RST}\n"
    printf "${CYN}%s${RST}\n" "════════════════════════════════════════════"

    trap 'printf "\n\n  ${GRN}Exiting live view.${RST}\n\n"; trap - INT; return' INT

    while true; do
        printf "\033[3;1H"
        local now=$(date '+%H:%M:%S')
        printf "${BOLD}${WHT}  SYNDICATE LIVE VIEW${RST}  ${DIM}%s${RST}        \n" "$now"
        printf "${CYN}%s${RST}\n" "════════════════════════════════════════════"

        for d in $ENABLED; do
            local f="$RESULTS/${d}.json"
            if [ -f "$f" ]; then
                local ts=$(jq -r '.timestamp // "?"' "$f" 2>/dev/null | grep -oE '[0-9]{2}:[0-9]{2}' | head -1)
                local extra=""
                case "$d" in
                    leatherheadd) extra=$(jq -r '"score=\(.thermal_score // "?")"' "$f" 2>/dev/null) ;;
                    rahzerd)      extra=$(jq -r '"\(.wifi.connected // "?") wifi \(.ports.established_tcp4 // 0)tcp"' "$f" 2>/dev/null) ;;
                    tigerclawd)   extra=$(jq -r '"trust=\(.trust_score // "?")"' "$f" 2>/dev/null) ;;
                    ratkingd)     extra=$(jq -r '"zombies=\(.processes.zombies // 0)"' "$f" 2>/dev/null) ;;
                    granitord)    extra=$(jq -r '"\(.posture.grade // "?")"' "$f" 2>/dev/null) ;;
                    shredderd)    extra=$(jq -r '"\(.integrity.grade // "?") new=\((.drift.new_modules // []) | length)"' "$f" 2>/dev/null) ;;
                    burned)       extra=$(jq -r '"signals=\(.privacy_signal_count // 0)"' "$f" 2>/dev/null) ;;
                    bebopd)       extra=$(jq -r '"drain=\(.drain_mah_h // .drain_mah_per_hour // 0)mAh"' "$f" 2>/dev/null) ;;
                    nulld)        extra=$(jq -r '"\(.screen // "?") spike=\(.idle_spike_detected // false)"' "$f" 2>/dev/null) ;;
                    overlordd)    extra=$(jq -r '"\(.correlation.status // "?") x\(.correlation.anomaly_count // 0)"' "$f" 2>/dev/null) ;;
                esac
                printf "  ${GRN}●${RST} ${CYN}%-14s${RST} ${DIM}%s${RST} ${WHT}%s${RST}          \n" "${DAEMON_NAME[$d]:-$d}" "$ts" "$extra"
            else
                printf "  ${DIM}○ %-14s no data${RST}          \n" "${DAEMON_NAME[$d]:-$d}"
            fi
        done
        sleep 3
    done

    trap - INT
}


# ═══════════════════════════════════════════════════════════════════════════════
# TOGGLE DAEMONS — enable/disable which daemons participate in scans
# ═══════════════════════════════════════════════════════════════════════════════
toggle_daemons_menu() {
    local list=()
    for d in $ALL_DAEMONS; do list+=("$d"); done
    local total=${#list[@]}

    while true; do
        clear
        printf "${BOLD}${WHT}"
        printf "╔══════════════════════════════════════════════════════════════════════╗\n"
        printf "║  TOGGLE DAEMONS                                                       ║\n"
        printf "╚══════════════════════════════════════════════════════════════════════╝\n"
        printf "${RST}\n"

        for i in "${!list[@]}"; do
            local d="${list[$i]}"
            local mark="${RED}✘ off${RST}"
            for e in $ENABLED; do [ "$e" = "$d" ] && mark="${GRN}✔ on ${RST}"; done
            printf "  ${WHT}%2d)${RST} %-14s %b\n" "$((i+1))" "${DAEMON_NAME[$d]:-$d}" "$mark"
        done

        printf "\n  ${DIM}Enter a number to toggle · [a] all on · [z] all off · [s] save & exit${RST}\n"
        printf "  Choice: "
        read -r key

        case "$key" in
            a|A) ENABLED="$ALL_DAEMONS" ;;
            z|Z) ENABLED="" ;;
            s|S) save_config; return ;;
            ''|*[!0-9]*) : ;;
            *)
                if [ "$key" -ge 1 ] 2>/dev/null && [ "$key" -le "$total" ]; then
                    local d="${list[$((key-1))]}"
                    local found=0 newlist=""
                    for e in $ENABLED; do
                        if [ "$e" = "$d" ]; then found=1; else newlist="$newlist $e"; fi
                    done
                    if [ $found -eq 1 ]; then
                        ENABLED="$(echo "$newlist" | xargs)"
                    else
                        ENABLED="$(echo "$ENABLED $d" | xargs)"
                    fi
                fi
                ;;
        esac
    done
}

# ═══════════════════════════════════════════════════════════════════════════════
# MAIN MENU
# ═══════════════════════════════════════════════════════════════════════════════
main() {
    consent_gate
    load_config

    while true; do
        clear
        printf "\n${BOLD}${WHT}"
        printf "╔══════════════════════════════════════════════════════════════════════╗\n"
        printf "║           MIUISERPERUSER — SYNDICATE SCAN                            ║\n"
        printf "║           Rolling (parallel) · Paged · Fast · Live                   ║\n"
        printf "╚══════════════════════════════════════════════════════════════════════╝\n"
        printf "${RST}\n"
        printf "  ${CYN}[1]${RST} ${WHT}Rolling Scan${RST}          ${DIM}(all enabled, shows skips)${RST}\n"
        printf "  ${CYN}[2]${RST} ${WHT}Rolling Scan (available)${RST} ${DIM}(only daemons with binaries)${RST}\n"
        printf "  ${CYN}[3]${RST} ${WHT}Fast Scan (parallel)${RST}   ${DIM}(all at once, meaty summary)${RST}\n"
        printf "  ${CYN}[4]${RST} ${WHT}Loop N fast scans${RST}     ${DIM}(parallel batch repeated)${RST}\n"
        printf "  ${CYN}[5]${RST} ${WHT}View Results${RST}          ${DIM}(paged per-daemon, prev/next)${RST}\n"
        printf "  ${CYN}[6]${RST} ${WHT}Live View${RST}             ${DIM}(dashboard, updates every 3s)${RST}\n"
        printf "  ${CYN}[7]${RST} ${WHT}Toggle Daemons${RST}        ${DIM}(enable/disable which daemons run)${RST}\n"
        printf "  ${CYN}[q]${RST} ${WHT}Quit${RST}\n\n"
        printf "  ${CYN}Choice: ${RST}"
        read -r choice
        case "$choice" in
            1)
                rolling_scan 0
                ;;
            2)
                rolling_scan 1
                ;;
            3)
                run_fast_scan 1
                printf "\n  ${CYN}Press any key to return to menu...${RST}"
                read -n1
                ;;
            4)
                read -r -p "$(printf "  ${CYN}How many times? ${RST}")" n
                for i in $(seq 1 "$n"); do
                    run_fast_scan "$i"
                    [ "$i" -lt "$n" ] && {
                        printf "\n  ${DIM}Next scan in 5s...${RST}\n"
                        sleep 5
                    }
                done
                printf "\n  ${GRN}✔ ${n} scans complete.${RST}\n"
                printf "  ${CYN}Press any key to return to menu...${RST}"
                read -n1
                ;;
            5)
                show_paged_results
                ;;
            6)
                live_view
                ;;
            7)
                toggle_daemons_menu
                ;;
            q|Q)
                clear
                printf "\n  ${GRN}Good hunting.${RST}\n\n"
                exit 0
                ;;
        esac
    done
}

main
