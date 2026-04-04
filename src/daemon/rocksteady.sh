#!/bin/bash
SEWER="/data/data/com.termux/files/home/.syndicate_sewer"
while true; do
    # Get the 1-minute load average
    LOAD=$(uptime | awk -F'load average:' '{ print $2 }' | cut -d, -f1 | sed 's/ //g')
    
    # Convert load (e.g., 2.50) to a rough percentage (assuming 8 cores)
    # If load is 8.00 on 8 cores, that is 100%
    CORES=$(nproc)
    PRESSURE=$(echo "$LOAD $CORES" | awk '{printf "%d", ($1/$2) * 100}')
    
    [[ -z "$PRESSURE" ]] && PRESSURE=0
    echo "pressure=$PRESSURE" > "$SEWER/rocksteady.packet"
    sleep 5
done
