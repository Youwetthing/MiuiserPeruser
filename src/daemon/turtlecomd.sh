#!/bin/bash
# TURTLECOMD: The Nerve & Routing Sentinel

SEWER="/data/data/com.termux/files/home/.syndicate_sewer"
mkdir -p "$SEWER"

echo "[TURTLECOMD] Nerve Center Online. Routing traffic..."

while true; do
    # 1. Cleanup Stale Handshakes
    # If a command (q.ready) is older than 30 seconds, it's a "Dead Letter"
    find "$SEWER" -name "q.ready" -mmin +0.5 -delete 2>/dev/null

    # 2. Monitor Pipe Integrity
    if [ ! -w "$SEWER" ]; then
        echo "[TURTLECOMD] ERROR: Sewer is read-only! Repairing..."
        chmod 700 "$SEWER"
    fi

    # 3. Heartbeat Routing
    # Turtlecomd can "touch" a heartbeat file so the Brain knows the network is up
    touch "$SEWER/turtlecom.heartbeat"

    sleep 5
done
