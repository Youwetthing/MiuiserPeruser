#!/data/data/com.termux/files/usr/bin/bash
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../env.sh"

BASE="$BASE"
LAW="$BASE/law_and_order:adb"
JAIL="$BASE/state/jailhouse/registry"
VP="$BASE/state/visitors_pass"
EVT="$BASE/state/court.events"

source "$LAW/court_event_lib.sh"

mkdir -p "$VP"

issue_pass() {
    local name="$1"
    local duration="$2"
    local reason="$3"

    (flock -x 200
        echo "$name|VISITOR|$reason|$(date +%s)|$duration" >> "$VP/pass_registry"
    ) 200>"$VP/pass_registry.lock"

    emit_event "VP" "ISSUED" "$name:$reason:duration=${duration}s"
}

revoke_pass() {
    local name="$1"

    grep -v "^$name|" "$VP/pass_registry" > "$VP/tmp" 2>/dev/null
    mv "$VP/tmp" "$VP/pass_registry" 2>/dev/null

    emit_event "VP" "REVOKED" "$name"
}

# Entry point if run directly
if [ -n "$1" ]; then
    case "$1" in
        issue)  issue_pass "$2" "$3" "$4" ;;
        revoke) revoke_pass "$2" ;;
        *)      echo "Usage: $0 {issue <name> <duration_sec> <reason>|revoke <name>}" ;;
    esac
fi
