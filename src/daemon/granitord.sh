#!/bin/bash
# GRANITORD: Final Sentinel Configuration

DAEMONS="powerhouse_daemon shizuku_intel.sh shredderd.sh fugitoidd.sh krangd.sh splinterd.sh"

while true; do
    for DAEMON in $DAEMONS; do
        if ! pgrep -f "$DAEMON" > /dev/null; then
            if [ "$DAEMON" = "powerhouse_daemon" ]; then
                cd ~/MiuiserPeruser/build && ./src/daemon/powerhouse_daemon > /dev/null 2>&1 &
            else
                sh ~/MiuiserPeruser/src/daemon/$DAEMON > /dev/null 2>&1 &
            fi
        fi
    done
    sleep 10
done
