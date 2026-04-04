#!/bin/bash
SEWER="/data/data/com.termux/files/home/.syndicate_sewer"
while true; do
    # Use dumpsys to find the CPU temperature
    # Note: This requires 'termux-chroot' or ADB shell permissions
    TEMP=$(dumpsys thermalservice | grep -m 1 "type=CPU" | cut -d= -f2 | cut -d. -f1 | awk '{print $1}')
    
    # Fallback: if dumpsys is empty, try the battery sensor (usually available)
    if [[ -z "$TEMP" || "$TEMP" -eq 0 ]]; then
        TEMP=$(dumpsys battery | grep "temperature" | awk '{print $2}')
        TEMP=$((TEMP / 10)) # Battery temp is usually 350 for 35.0C
    fi

    echo "fever=${TEMP:-0}" > "$SEWER/leatherheadd.packet"
    sleep 10
done
