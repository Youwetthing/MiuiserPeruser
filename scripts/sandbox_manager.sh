#!/data/data/com.termux/files/usr/bin/bash
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../env.sh"

BASE="$BASE"
SANDBOX="$BASE/state/sandbox"

mkdir -p "$SANDBOX"

sandbox_process() {
    target="$1"

    mkdir -p "$SANDBOX/$target"

    echo "🧱 SANDBOXING: $target"

    # move logs / artifacts if they exist
    find "$BASE" -name "*$target*" 2>/dev/null > "$SANDBOX/$target/files.txt"

    echo "$(date +%s)|SANDBOX|ISOLATED|$target" >> "$BASE/state/court.events"
}

release_sandbox() {
    target="$1"

    rm -rf "$SANDBOX/$target"
    echo "$(date +%s)|SANDBOX|RELEASED|$target" >> "$BASE/state/court.events"
}

