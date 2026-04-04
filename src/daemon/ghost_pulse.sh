#!/bin/sh
# ghost_pulse.sh - Passive Saturation Observer
# Reports true kernel load vs core count.

. $HOME/MiuiserPeruser/config.cfg

while true; do
    # Sample the 1-minute load average
    L=$(uptime | awk -F'load average:' '{print $2}' | cut -d, -f1 | xargs)
    
    # Calculate Saturation % relative to CPU cores
    # Using integer math for shell compatibility
    SAT=$(( (${L%.*} * 100) / $CPU_CORES ))
    
    # Write the Packet
    echo "pressure=$SAT" > "$PACKET_FILE"
    echo "[$(date +%T)] OBSERVED SATURATION: $SAT%" >> "$LOG_FILE"
    
    sleep "$MONITOR_INTERVAL"
done
