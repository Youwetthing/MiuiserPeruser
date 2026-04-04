#!/bin/sh
SEWER="/data/data/com.termux/files/home/.syndicate_sewer"

while true; do
    # One single call to get the data
    RAW=$(rish -c "uptime; dumpsys battery" 2>/dev/null)
    
    # 1. PARSE LOAD (Pure Shell Logic)
    # Trim everything before "average: "
    PRE_LOAD="${RAW#*average: }"
    # Trim everything after the first comma
    LOAD_1M="${PRE_LOAD%%,*}"
    # Basic math: Integer part * 100 / 8 (approx scaling)
    INT_LOAD="${LOAD_1M%.*}"
    [ -z "$INT_LOAD" ] && INT_LOAD=0
    CPU_SAT=$(( (INT_LOAD * 100) / 8 ))

    # 2. PARSE BATT (Pure Shell Logic)
    PRE_BATT="${RAW#*level: }"
    BATT_LEVEL="${PRE_BATT%%scale:*}"
    BATT_LEVEL=$(echo $BATT_LEVEL | xargs)

    # 3. WRITE PACKETS
    echo "pressure=${CPU_SAT:-0}" > "$SEWER/rocksteady.packet"
    echo "cap=${BATT_LEVEL:-30}" > "$SEWER/bebopd.packet"
    echo "fever=35" > "$SEWER/leatherheadd.packet"

    sleep 15
done
