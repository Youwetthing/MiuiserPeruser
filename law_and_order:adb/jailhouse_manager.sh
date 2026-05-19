#!/data/data/com.termux/files/usr/bin/bash
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../env.sh"

BASE="$BASE"
LAW="$BASE/law_and_order:adb"
JAIL="$BASE/state/jailhouse"

source "$LAW/judicial_firewall.sh"
source "$LAW/court_event_lib.sh"
EVT="$BASE/state/court.events"

mkdir -p "$JAIL"

jail() {
    name="$1"
    reason="$2"
    allow_action ISOLATE || { echo "🚫 Firewall blocked JAIL: $name"; return 1; }

    # remove duplicates
    grep -v "^$name|" "$JAIL/registry" > "$JAIL/tmp" 2>/dev/null
    mv "$JAIL/tmp" "$JAIL/registry" 2>/dev/null

    (flock -x 200; echo "$name|JAIL|$reason|$(date +%s)" >> "$JAIL/registry") 200>"$JAIL/registry.lock"
    (flock -x 200; echo "$(date +%s)|JAIL|ENTER|$name:$reason" >> "$EVT") 200>"$EVT.lock"
}

release() {
    name="$1"
    allow_action RESTART || { echo "🚫 Firewall blocked RELEASE: $name"; return 1; }

    grep -v "^$name|" "$JAIL/registry" > "$JAIL/tmp" 2>/dev/null
    mv "$JAIL/tmp" "$JAIL/registry" 2>/dev/null

    (flock -x 200; echo "$(date +%s)|JAIL|RELEASE|$name" >> "$EVT") 200>"$EVT.lock"
}
