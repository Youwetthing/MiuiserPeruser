#!/bin/bash
SEWER="/data/data/com.termux/files/home/.syndicate_sewer"

while true; do
    # Read the Stress Score (Load + I/O Wait)
    STRESS=$(cat "$SEWER/rocksteady.packet" | cut -d= -f2)
    
    # If Stress > 200 (Extreme saturation), trigger Decompression
    if [[ $STRESS -gt 200 ]]; then
        # Identify the top USER app (not system_server) eating CPU
        # We target the most recent 'major fault' heavy hitters
        TARGET=$(rish -c "dumpsys cpuinfo" | grep "% " | grep -v "system_server" | head -n 1 | awk '{print $NF}' | cut -d: -f1)
        
        if [[ ! -z "$TARGET" ]]; then
            # We use 'am force-stop' to completely clear the app's memory footprint
            echo "$TARGET" > "$SEWER/q.ready"
        fi
        
        # Force a Trim Memory event to tell system_server to relax
        rish -c "am kill-all"
    fi
    sleep 20
done
