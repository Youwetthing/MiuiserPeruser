#!/bin/bash
SEWER="/data/data/com.termux/files/home/.syndicate_sewer"
while true; do
    # Try sysfs first (Internal kernel path)
    CAP=$(cat /sys/class/power_supply/battery/capacity 2>/dev/null)
    # Fallback to termux-battery-status
    [[ -z "$CAP" ]] && CAP=$(termux-battery-status | grep percentage | awk '{print $2}' | tr -d ',')
    
    echo "cap=${CAP:-7}" > "$SEWER/bebopd.packet"
    sleep 30
done
