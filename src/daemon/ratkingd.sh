#!/bin/bash
# RATKINGD: The Crowd Sentinel (Process Profiler)

SEWER="/data/data/com.termux/files/home/.syndicate_sewer"
mkdir -p "$SEWER"

while true; do
    # Get the top process (excluding internal daemons and system overhead)
    # Batch mode (-b) for one-shot output
    TOP_PROC=$(top -b -n 1 | grep -ivE "top|grep|sh|powerhouse" | head -n 25 | sort -rk 9 | head -n 1)

    PID=$(echo $TOP_PROC | awk '{print $1}')
    CPU=$(echo $TOP_PROC | awk '{print $9}')
    NAME=$(echo $TOP_PROC | awk '{print $12}')

    # Default values if empty
    [[ -z "$PID" ]] && PID=0
    [[ -z "$CPU" ]] && CPU=0.0
    [[ -z "$NAME" ]] && NAME="idle"

    # Write the Packet
    echo "pid=$PID|name=$NAME|cpu=$CPU" > "$SEWER/ratkingd.packet.tmp"
    mv "$SEWER/ratkingd.packet.tmp" "$SEWER/ratkingd.packet"

    sleep 5
done
