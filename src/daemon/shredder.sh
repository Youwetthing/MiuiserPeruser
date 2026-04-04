#!/bin/bash
SEWER="/data/data/com.termux/files/home/.syndicate_sewer"
HISTORY="/data/data/com.termux/files/home/syndicate.history"

while true; do
    if [[ -f "$SEWER/q.ready" ]]; then
        TARGET=$(cat "$SEWER/q.ready")
        if [[ ! -z "$TARGET" ]]; then
            # 1. Kill the specific target
            rish -c "pkill -f $TARGET"
            echo "[$(date '+%Y-%m-%d %H:%M:%S')] [SHREDDER] Reaped: $TARGET" >> "$HISTORY"
            
            # 2. If RAM is low, drop caches (The "Sewer Flush")
            # Note: This is a safe rish command to trim background apps
            rish -c "am kill-all" 
            echo "[$(date '+%Y-%m-%d %H:%M:%S')] [SHREDDER] Global RAM Flush triggered." >> "$HISTORY"
        fi
        rm "$SEWER/q.ready"
    fi
    sleep 5
done
