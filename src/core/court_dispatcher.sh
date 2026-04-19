#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"
EVT="$BASE/state/court.events"

emit_case() {
    entity="$1"
    severity="$2"
    reason="$3"

    echo "$(date +%s)|CRE|CASE|$entity:$severity:$reason" >> "$EVT"
}

emit_case "$1" "$2" "$3"
