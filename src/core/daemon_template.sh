#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"
source "$BASE/src/core/event_bus.sh"

DAEMON_NAME="$1"

if [ -z "$DAEMON_NAME" ]; then
    echo "Usage: $0 <daemon_name>"
    exit 1
fi

emit_event "$DAEMON_NAME" "HEARTBEAT" "STARTED"

while true; do
    # heartbeat tick
    emit_event "$DAEMON_NAME" "HEARTBEAT" "OK"
    sleep 10
done
