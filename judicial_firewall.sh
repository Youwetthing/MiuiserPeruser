#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"
EVT="$BASE/state/court.events"

allow_action() {
    action="$1"

    case "$action" in
        RESTART|ISOLATE|THROTTLE)
            return 0
            ;;
        KILL|WIPE|FORMAT)
            echo "🚫 BLOCKED BY FIREWALL: $action" >> "$EVT"
            return 1
            ;;
        *)
            return 0
            ;;
    esac
}
