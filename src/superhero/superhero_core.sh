#!/data/data/com.termux/files/usr/bin/bash

# SUPERHERO MODE
# Behaviour interpretation only

source "$(dirname "$0")/../../env.sh"

INPUT="$SYNDICATE_FEED_SOCK"
OUTPUT="$SUPERHERO_SOCK"
LOG="$LOGS/superhero.log"

log() {
    echo "[SUPERHERO] $1" >> "$LOG"
}

evaluate() {
    local data="$1"

    # placeholder anomaly scoring
    if [[ "$data" == *"ANOMALY"* ]]; then
        echo "HIGH|Syndicate|Detected anomaly" >> "$OUTPUT"
        log "ANOMALY escalated"
    else
        echo "LOW|Syndicate|Normal" >> "$OUTPUT"
        log "normal state"
    fi
}

while true; do
    if read line < "$INPUT"; then
        evaluate "$line"
    fi
done
