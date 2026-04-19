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

    echo "$name|$state|$pid" >> "$REG"
}

get_registry() {
    cat "$REG"
}
