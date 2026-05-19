#!/data/data/com.termux/files/usr/bin/bash
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../env.sh"

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
