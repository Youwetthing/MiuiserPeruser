#!/data/data/com.termux/files/usr/bin/bash

EVENT_FILE="$HOME/MiuiserPeruser/state/court.events"

emit_event() {
    local type="$1"
    local layer="$2"
    local msg="$3"
    local ts=$(date +%s)

    echo "${ts}|${layer}|${type}|${msg}" >> "$EVENT_FILE"
}
