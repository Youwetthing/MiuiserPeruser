#!/data/data/com.termux/files/usr/bin/bash
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../env.sh"
MAX_LOG_BYTES=524288  # 500KB

_rotate_log() {
    local f="${1:-$LOG}"
    if [ -f "$f" ] && [ "$(stat -c%s "$f" 2>/dev/null || echo 0)" -gt "${MAX_LOG_BYTES:-524288}" ]; then
        mv "$f" "${f}.1"
        > "$f"
    fi
}


BASE="$BASE"
EVT="$BASE/state/court.events"

emit_event() {
    source="$1"
    event="$2"
    detail="$3"

    (flock -x 200; echo "$(date +%s)|$source|$event|$detail" >> "$EVT") 200>"$EVT.lock"
}
