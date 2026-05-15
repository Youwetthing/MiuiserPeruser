#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"
STATE_FILE="$BASE/state/cases.state"

mkdir -p "$BASE/state"

update_state() {
    echo "$1|$2" >> "$STATE_FILE"
}

get_state() {
    grep "$1" "$STATE_FILE" | tail -n 1
}

