#!/data/data/com.termux/files/usr/bin/bash

# SYNDICATE: SYSTEM INTELLIGENCE LAYER
# Deep scans, raw data only

LOG="/data/data/com.termux/files/home/MiuiserPeruser/logs/syndicate.log"

log() {
    echo "[SYNDICATE] $1" >> "$LOG"
}

scan() {
    log "SCAN | $1"
    echo "$1"
}

