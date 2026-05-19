#!/data/data/com.termux/files/usr/bin/bash
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../env.sh"

BASE="$BASE"
LEDGER="$BASE/state/criminal_record/ledger.log"
mkdir -p "$(dirname "$LEDGER")"

record() {
    entity="$1"
    verdict="$2"
    reason="$3"
    source="$4"

    (flock -x 200; echo "$(date +%s)|$entity|$verdict|$reason|$source" >> "$LEDGER") 200>"$LEDGER.lock"
}

if [ "$1" ]; then
    record "$1" "$2" "$3" "$4"
else
    echo "Usage: record <entity> <verdict> <reason> <source>"
fi
