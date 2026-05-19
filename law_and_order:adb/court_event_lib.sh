#!/data/data/com.termux/files/usr/bin/bash
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../env.sh"

BASE="$BASE"
EVT="$BASE/state/court.events"

emit_event() {
    source="$1"
    event="$2"
    detail="$3"

    (flock -x 200; echo "$(date +%s)|$source|$event|$detail" >> "$EVT") 200>"$EVT.lock"
}
