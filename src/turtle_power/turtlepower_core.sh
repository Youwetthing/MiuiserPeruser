#!/data/data/com.termux/files/usr/bin/bash

# TURTLEPOWER - JUDICIAL SYSTEM
# ONLY decision making

source "$(dirname "$0")/../../env.sh"

LOG="$TURTLEPOWER_LOG"
INPUT="$SUPERHERO_SOCK"
MAX_LOG_BYTES=5242880  # 5MB

log() {
    if [ -f "$LOG" ] && [ "$(stat -c%s "$LOG" 2>/dev/null || echo 0)" -gt "$MAX_LOG_BYTES" ]; then
        mv "$LOG" "${LOG}.1"
    fi
    echo "[TURTLEPOWER] $1" >> "$LOG"
}

decide() {
    local severity="$1"
    local source="$2"
    local detail="$3"

    case "$severity" in
        LOW)      log "IGNORE | $source | $detail" ;;
        MEDIUM)   log "WARN | $source | $detail" ;;
        HIGH)     log "THROTTLE | $source | $detail" ;;
        CRITICAL) log "ESCALATE | $source | $detail" ;;
        KILL)     log "ISOLATE | $source | $detail" ;;
    esac
}

while true; do
    if read line < "$INPUT"; then
        IFS="|" read -r sev src det <<< "$line"
        decide "$sev" "$src" "$det"
    fi
done
