#!/data/data/com.termux/files/usr/bin/bash

# SUPERHERO ADAPTER
# Parses superhero stderr output → src|sig|score|ctx → superhero.pipe

BASE="$HOME/MiuiserPeruser"
PIPE="$BASE/pipes/superhero.pipe"
LOG="$BASE/logs/superhero_adapter.log"

log() { echo "[ADAPTER] $(date +%s) $1" >> "$LOG"; }

emit() {
    local sig="$1" score="$2" ctx="$3"
    local line="superherod|$sig|$score|$ctx"
    log "EMIT: $line"
    echo "$line" > "$PIPE"
}

log "ONLINE"

declare -A EMITTED
SCAN_ID=0

"$BASE/Superhero_Mode/superhero" --loop 30 2>&1 | while IFS= read -r line; do

    # Reset seen signals on each new scan cycle
    if [[ "$line" =~ "scan_start" ]]; then
        unset EMITTED
        declare -A EMITTED
        SCAN_ID=$((SCAN_ID + 1))
        log "New scan cycle $SCAN_ID"
    fi

    # Battery health
    if [[ "$line" =~ "Battery health estimated at "([0-9]+) ]]; then
        val="${BASH_REMATCH[1]}"
        [ "${EMITTED[BATTERY]}" = "1" ] && continue
        EMITTED[BATTERY]=1
        if [ "$val" -le 20 ]; then
            emit "BATTERY_HEALTH_LOW" 75 "health=${val}%"
        elif [ "$val" -le 40 ]; then
            emit "BATTERY_HEALTH_LOW" 45 "health=${val}%"
        fi
    fi

    # CPU throttling
    if [[ "$line" =~ "CPU_THROTTLING" ]]; then
        emit "CPU_THROTTLING" 60 "cores_throttled"
    fi

    # RWX memory page
    if [[ "$line" =~ "RWX_MEMORY_PAGE" ]]; then
        proc=$(echo "$line" | grep -o "\[.*\]" | head -1)
        emit "RWX_MEMORY_PAGE" 90 "process=$proc"
    fi

    # Network anomaly — explicit signal only
    if [[ "$line" =~ "NETWORK_ANOMALY" ]] || [[ "$line" =~ "ANOMALY_DETECTED" ]]; then
        [ "${EMITTED[NETWORK]}" = "1" ] && continue
        EMITTED[NETWORK]=1
        emit "NETWORK_ANOMALY" 70 "$line"
    fi

    # Hidden process — only fire on actual detection, not the clear message
    if [[ "$line" =~ "HIDDEN_PROCESS" ]] ||        ( [[ "$line" =~ "hidden" ]] && ! [[ "$line" =~ "no hidden" ]] ); then
        [ "${EMITTED[HIDDEN]}" = "1" ] && continue
        EMITTED[HIDDEN]=1
        proc=$(echo "$line" | grep -o "\[.*\]" | head -1)
        emit "HIDDEN_PROCESS" 85 "process=$proc"
    fi

    # Integrity violation
    if [[ "$line" =~ "INTEGRITY" ]] && [[ "$line" =~ "FAIL" ]]; then
        emit "INTEGRITY_VIOLATION" 95 "$line"
    fi

    # MIUI game turbo active (MIUI bypassing restrictions)
    if [[ "$line" =~ "game_turbo=1" ]]; then
        emit "MIUI_GAME_TURBO" 50 "game_turbo_active"
    fi

done
