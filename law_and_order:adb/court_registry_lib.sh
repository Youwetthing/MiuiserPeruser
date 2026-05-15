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
