#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"
LEDGER="$BASE/state/criminal_record/ledger.log"

record() {
    entity="$1"
    verdict="$2"
    reason="$3"
    source="$4"

    echo "$(date +%s)|$entity|$verdict|$reason|$source" >> "$LEDGER"
}

if [ "$1" ]; then
    record "$1" "$2" "$3" "$4"
else
    echo "Usage: record <entity> <verdict> <reason> <source>"
fi
