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
STATE_FILE="$BASE/state/cases.state"

mkdir -p "$BASE/state"

update_state() {
    echo "$1|$2" >> "$STATE_FILE"
}

get_state() {
    grep "$1" "$STATE_FILE" | tail -n 1
}

