#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"
JAIL="$BASE/state/jailhouse/registry"
VP="$BASE/state/visitors_pass"

mkdir -p "$VP"

issue_pass() {
    name="$1"
    duration="$2"  # seconds
    reason="$3"

    echo "$name|VISITOR|$reason|$(date +%s)|$duration" >> "$VP/pass_registry"
    echo "$(date +%s)|VP|ISSUED|$name:$reason" >> "$BASE/state/court.events"
}

revoke_pass() {
    name="$1"

    grep -v "^$name|" "$VP/pass_registry" > "$VP/tmp" 2>/dev/null
    mv "$VP/tmp" "$VP/pass_registry" 2>/dev/null

    echo "$(date +%s)|VP|REVOKED|$name" >> "$BASE/state/court.events"
}

