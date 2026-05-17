#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"
REG="$BASE/state/court.registry"

mkdir -p "$BASE/state"

register() {
    name="$1"
    state="$2"
    pid="$3"

    grep -v "^$name|" "$REG" > "$REG.tmp" 2>/dev/null
    mv "$REG.tmp" "$REG" 2>/dev/null

    (flock -x 200; echo "$name|$state|$pid" >> "$REG") 200>"$REG.lock"
}

get_registry() {
    cat "$REG"
}

EVT="$BASE/state/court.events"
EVT_ROTATE_THRESHOLD=2000

rotate_court_events() {
    local lines
    lines=$(wc -l < "$EVT" 2>/dev/null || echo 0)
    [ "$lines" -lt "$EVT_ROTATE_THRESHOLD" ] && return 0

    local archive="$EVT.$(date +%Y%m%d_%H%M%S)"
    (flock -x 201
     mv "$EVT" "$archive"
     touch "$EVT"
    ) 201>"$EVT.rotate.lock"
    echo "[court_registry_lib] court.events rotated → $archive (was $lines lines)"
}

EVT="$BASE/state/court.events"
EVT_ROTATE_THRESHOLD=2000

rotate_court_events() {
    local lines
    lines=$(wc -l < "$EVT" 2>/dev/null || echo 0)
    [ "$lines" -lt "$EVT_ROTATE_THRESHOLD" ] && return 0

    local archive="$EVT.$(date +%Y%m%d_%H%M%S)"
    (flock -x 201
     mv "$EVT" "$archive"
     touch "$EVT"
    ) 201>"$EVT.rotate.lock"
    echo "[court_registry_lib] court.events rotated → $archive (was $lines lines)"
}
