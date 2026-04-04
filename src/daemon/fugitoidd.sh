#!/bin/bash
SEWER="/data/data/com.termux/files/home/.syndicate_sewer"
HISTORY="/data/data/com.termux/files/home/syndicate.history"

while true; do
    TS=$(date '+%Y-%m-%d %H:%M:%S')
    CPU=$(grep "pressure=" "$SEWER/rocksteady.packet" | cut -d= -f2 || echo 0)
    TEMP=$(grep "fever=" "$SEWER/leatherheadd.packet" | cut -d= -f2 || echo 0)
    BATT=$(grep "cap=" "$SEWER/bebopd.packet" | cut -d= -f2 | cut -d'|' -f1 || echo 0)
    
    echo "[$TS] CPU:$CPU% | TEMP:$TEMP°C | BATT:$BATT%" >> "$HISTORY"
    sleep 60
done
