#!/data/data/com.termux/files/usr/bin/bash
# SUPERHERO ADAPTER
# Parses superhero binary stderr → src|sig|score|ctx → superhero.pipe
#
# FIX: pipe | while ran in subshell — EMITTED and SCAN_ID reset every line.
#      Now uses process substitution so loop runs in current shell.

BASE="$HOME/MiuiserPeruser"
PIPE="$BASE/pipes/superhero.pipe"
LOG="$BASE/logs/superhero_adapter.log"

log() { echo "[ADAPTER] $(date +%s) $1" >> "$LOG"; }

emit() {
    local sig="$1" score="$2" ctx="$3" src="${4:-superherod}"
    local line="$src|$sig|$score|$ctx"
    log "EMIT: $line"
    ( echo "$line" > "$PIPE" ) &
    disown $!
}

log "ONLINE"

declare -A EMITTED
SCAN_ID=0

# Outer restart loop — relaunches superhero binary if it exits
while true; do

# FIX: process substitution keeps loop in current shell — EMITTED persists
while IFS= read -r line; do

    if [[ "$line" =~ "scan_start" ]]; then
        unset EMITTED
        declare -A EMITTED
        SCAN_ID=$((SCAN_ID + 1))
        log "New scan cycle $SCAN_ID"
    fi

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

    if [[ "$line" =~ "CPU_THROTTLING" ]]; then
        [ "${EMITTED[CPU_THROTTLE]}" = "1" ] && continue
        EMITTED[CPU_THROTTLE]=1
        emit "CPU_THROTTLING" 60 "cores_throttled"
    fi

    if [[ "$line" =~ "RWX_MEMORY_PAGE" ]]; then
        proc=$(echo "$line" | grep -o "\[[^]]*\]" | head -1 | tr -d '[]')
        src="${proc:-superherod}"
        emit "RWX_MEMORY_PAGE" 90 "process=$proc" "$src"
    fi

    if [[ "$line" =~ "NETWORK_ANOMALY" ]] || [[ "$line" =~ "ANOMALY_DETECTED" ]]; then
        [ "${EMITTED[NETWORK]}" = "1" ] && continue
        EMITTED[NETWORK]=1
        emit "NETWORK_ANOMALY" 70 "$line"
    fi

    if [[ "$line" =~ "HIDDEN_PROCESS" ]] || \
       ( [[ "$line" =~ "hidden" ]] && ! [[ "$line" =~ "no hidden" ]] ); then
        [ "${EMITTED[HIDDEN]}" = "1" ] && continue
        EMITTED[HIDDEN]=1
        proc=$(echo "$line" | grep -o "\[[^]]*\]" | head -1 | tr -d '[]')
        src="${proc:-superherod}"
        emit "HIDDEN_PROCESS" 85 "process=$proc" "$src"
    fi

    if [[ "$line" =~ "INTEGRITY" ]] && [[ "$line" =~ "FAIL" ]]; then
        emit "INTEGRITY_VIOLATION" 95 "$line"
    fi

    if [[ "$line" =~ "game_turbo=1" ]]; then
        emit "MIUI_GAME_TURBO" 50 "game_turbo_active"
    fi

done < <("$BASE/Superhero_Mode/superhero" --loop 30 2>&1)

log "superhero exited — restarting in 5s"
sleep 5

done  # outer restart loop
