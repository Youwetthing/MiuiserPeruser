#!/bin/bash
# METALHEADD: The I/O & Storage Sentinel

SEWER="/data/data/com.termux/files/home/.syndicate_sewer"
mkdir -p "$SEWER"

echo "[METALHEADD] Sentinel Online. Monitoring The Thirst..."

# Track previous values to calculate delta
PREV_IDLE=0
PREV_IOW=0

while true; do
    # 1. Get I/O Wait from /proc/stat (5th column of the 'cpu' line)
    CPU_LINE=$(grep '^cpu ' /proc/stat)
    IOW=$(echo $CPU_LINE | awk '{print $6}')
    IDLE=$(echo $CPU_LINE | awk '{print $5}')
    
    # 2. Calculate simple % change (Pressure)
    DIFF_IOW=$((IOW - PREV_IOW))
    DIFF_IDLE=$((IDLE - PREV_IDLE))
    TOTAL=$((DIFF_IOW + DIFF_IDLE))
    
    if [ $TOTAL -gt 0 ]; then
        IOW_PERCENT=$(( (100 * DIFF_IOW) / TOTAL ))
    else
        IOW_PERCENT=0
    fi

    # 3. Get Free Space (Termux Home)
    FREE_MB=$(df -m /data/data/com.termux/files/home | tail -1 | awk '{print $4}')

    # 4. Write the Packet
    echo "io.wait=$IOW_PERCENT|disk.free=$FREE_MB" > "$SEWER/metalheadd.packet.tmp"
    mv "$SEWER/metalheadd.packet.tmp" "$SEWER/metalheadd.packet"

    PREV_IOW=$IOW
    PREV_IDLE=$IDLE
    sleep 3
done
