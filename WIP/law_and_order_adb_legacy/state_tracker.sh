#!/data/data/com.termux/files/usr/bin/bash
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../env.sh"
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../../lib/miuiserperuser_common.sh"

BASE="$BASE"
STATE_FILE="$BASE/state/cases.state"

mkdir -p "$BASE/state"

update_state() {
    echo "$1|$2" >> "$STATE_FILE"
}

get_state() {
    grep "$1" "$STATE_FILE" | tail -n 1
}

