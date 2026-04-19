#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"
JAIL="$BASE/state/jailhouse"
EVT="$BASE/state/court.events"

mkdir -p "$JAIL"

jail() {
    name="$1"
    reason="$2"

    # remove duplicates
    grep -v "^$name|" "$JAIL/registry" > "$JAIL/tmp" 2>/dev/null
    mv "$JAIL/tmp" "$JAIL/registry" 2>/dev/null

    echo "$name|JAIL|$reason|$(date +%s)" >> "$JAIL/registry"
    echo "$(date +%s)|JAIL|ENTER|$name:$reason" >> "$EVT"
}

release() {
    name="$1"

    grep -v "^$name|" "$JAIL/registry" > "$JAIL/tmp" 2>/dev/null
    mv "$JAIL/tmp" "$JAIL/registry" 2>/dev/null

    echo "$(date +%s)|JAIL|RELEASE|$name" >> "$EVT"
}
