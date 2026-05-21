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

allow_action() {
    action="$1"

    case "$action" in
        RESTART|ISOLATE|THROTTLE|QUARANTINE)
            return 0
            ;;
        KILL|WIPE|FORMAT)
            echo "🚫 BLOCKED BY FIREWALL: $action" >> "$EVT"
            return 1
            ;;
        *)
            echo "🚫 UNKNOWN ACTION BLOCKED: $action" >> "$EVT"
            return 1
            ;;
    esac
}
