#!/data/data/com.termux/files/usr/bin/bash

# TURTLEPOWER - JUDICIAL SYSTEM
# ONLY decision making

LOG="/data/data/com.termux/files/home/MiuiserPeruser/src/turtle_power/logs/turtlepower.log"
INPUT="/data/data/com.termux/files/home/MiuiserPeruser/pipes/superhero.pipe"

log() {
    echo "[TURTLEPOWER] $1" >> "$LOG"
}

decide() {
    local severity="$1"
    local source="$2"
    local detail="$3"

    case "$severity" in
        LOW)
            log "IGNORE | $source | $detail"
            ;;
        MEDIUM)
            log "WARN | $source | $detail"
            ;;
        HIGH)
            log "THROTTLE | $source | $detail"
            ;;
        CRITICAL)
            log "ESCALATE | $source | $detail"
            ;;
        KILL)
            log "ISOLATE | $source | $detail"
            ;;
    esac
}

exec 3<>"$INPUT"
while IFS="|" read -r src sig score ctx <&3; do
    [ -z "$src" ] && continue

    # Map score to severity
    if [ "$score" -ge 80 ]; then
        sev="CRITICAL"
    elif [ "$score" -ge 60 ]; then
        sev="HIGH"
    elif [ "$score" -ge 40 ]; then
        sev="MEDIUM"
    else
        sev="LOW"
    fi

    decide "$sev" "$src" "$sig:$ctx"
done

