#!/data/data/com.termux/files/usr/bin/bash

# SYNDICATE: SYSTEM INTELLIGENCE LAYER
# Deep scans, raw data only

source "$(dirname "$0")/../../env.sh"

LOG="$LOGS/syndicate.log"

log() {
    echo "[SYNDICATE] $1" >> "$LOG"
}

scan() {
    log "SCAN | $1"
    echo "$1"
}
