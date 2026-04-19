#!/data/data/com.termux/files/usr/bin/bash

# TURTLEPOWER - JUDICIAL SYSTEM
# ONLY decision making

LOG="/data/data/com.termux/files/home/MiuiserPeruser/src/turtle_power/logs/turtlepower.log"
INPUT="/data/data/com.termux/files/home/MiuiserPeruser/tmp/superhero.sock"

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

while true; do
    if read line < "$INPUT"; then
        IFS="|" read -r sev src det <<< "$line"
        decide "$sev" "$src" "$det"
    fi
done

