#!/data/data/com.termux/files/usr/bin/bash
# =============================================================
# court_dispatcher.sh
# Bridges turtlecomd APRIL stream → judicial superhero.pipe
#
# turtlecomd stdout is redirected to logs/turtlecomd.log by
# start_syndicate.sh. This daemon tails that log, picks up
# every APRIL| line, scores it, and writes:
#   SOURCE|SIGNAL|SCORE|CONTEXT
# to pipes/superhero.pipe for april_o_neil to assemble.
# =============================================================

BASE="$HOME/MiuiserPeruser"
TCOMD_LOG="$BASE/logs/turtlecomd.log"
PIPE="$BASE/pipes/superhero.pipe"
LOG="$BASE/logs/court_dispatcher.log"

mkdir -p "$BASE/logs"

log() { echo "[DISPATCHER] $(date +%s) $1" >> "$LOG"; }

emit() {
    local src="$1" signal="$2" score="$3" ctx="$4"
    local entry="$src|$signal|$score|$ctx"
    log "EMIT: $entry"
    # Non-blocking write — background fork so we never stall
    ( echo "$entry" > "$PIPE" ) &
    disown $!
}

# Score table — maps APRIL signal types to score + canonical signal name
score_signal() {
    local src="$1"
    local raw_signal="$2"
    local payload="$3"

    case "$raw_signal" in
        thermal_critical)
            # leatherheadd: peak_c=49.8 zone=CPU critical_zones=10
            local peak
            peak=$(echo "$payload" | grep -o 'peak_c=[0-9.]*' | cut -d= -f2)
            local zones
            zones=$(echo "$payload" | grep -o 'critical_zones=[0-9]*' | cut -d= -f2)
            if [ "${peak%.*}" -ge 55 ] 2>/dev/null; then
                emit "$src" "THERMAL_CRITICAL" 80 "$payload"
            elif [ "${zones:-0}" -ge 5 ] 2>/dev/null; then
                emit "$src" "THERMAL_CRITICAL" 70 "$payload"
            else
                emit "$src" "THERMAL_WARN" 40 "$payload"
            fi
            ;;

        cpu_hog)
            # ratkingd: pid=31092 name=com.anthropic.claude cpu_pct=56
            local pct
            pct=$(echo "$payload" | grep -o 'cpu_pct=[0-9]*' | cut -d= -f2)
            if [ "${pct:-0}" -ge 80 ] 2>/dev/null; then
                emit "$src" "CPU_HOG_CRITICAL" 75 "$payload"
            elif [ "${pct:-0}" -ge 50 ] 2>/dev/null; then
                emit "$src" "CPU_HOG" 50 "$payload"
            fi
            ;;

        cpu_spike)
            emit "$src" "CPU_THROTTLING" 60 "$payload"
            ;;

        netstate)
            # rahzerd: suspicious=N divergence=N
            local suspicious
            suspicious=$(echo "$payload" | grep -o 'suspicious=[0-9]*' | cut -d= -f2)
            local divergence
            divergence=$(echo "$payload" | grep -o 'divergence=[0-9]*' | cut -d= -f2)
            if [ "${suspicious:-0}" -gt 0 ] || [ "${divergence:-0}" -gt 0 ]; then
                emit "$src" "NETWORK_ANOMALY" 70 "$payload"
            fi
            # Clean netstate — no emit
            ;;

        anomaly|connectivity_anomaly)
            emit "$src" "NETWORK_ANOMALY" 70 "$payload"
            ;;

        wakelock)
            emit "$src" "WAKELOCK_ANOMALY" 40 "$payload"
            ;;

        sysstate)
            # granitord/shredderd: check score field
            local score_val
            score_val=$(echo "$payload" | grep -o 'SCORE=[0-9]*' | cut -d= -f2)
            if [ "${score_val:-100}" -lt 80 ] 2>/dev/null; then
                emit "$src" "INTEGRITY_VIOLATION" 95 "$payload"
            fi
            ;;

        *)
            # Unknown signal — log only, don't emit
            log "UNSCORED: src=$src signal=$raw_signal payload=$payload"
            ;;
    esac
}

log "ONLINE — waiting for $TCOMD_LOG"

# Wait for log file to exist (turtlecomd may not have started yet)
until [ -f "$TCOMD_LOG" ]; do
    sleep 1
done

log "Tailing $TCOMD_LOG"

# tail -f keeps reading as turtlecomd appends
# Pattern: [turtlecomd][INFO] Received: APRIL|source|signal|payload
tail -f "$TCOMD_LOG" | while IFS= read -r line; do

    # Only care about APRIL message lines
    [[ "$line" != *"Received: APRIL|"* ]] && continue

    # Extract the APRIL payload after "Received: "
    april="${line#*Received: APRIL|}"
    # april is now: source|signal|payload

    IFS='|' read -r src signal payload <<< "$april"

    [ -z "$src" ] || [ -z "$signal" ] && continue

    score_signal "$src" "$signal" "$payload"

done
