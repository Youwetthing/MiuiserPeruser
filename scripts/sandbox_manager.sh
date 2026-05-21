#!/data/data/com.termux/files/usr/bin/bash
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../env.sh"
MAX_LOG_BYTES=524288  # 500KB

_rotate_log() {
    local f="${1:-$LOG}"
    if [ -f "$f" ] && [ "$(stat -c%s "$f" 2>/dev/null || echo 0)" -gt "${MAX_LOG_BYTES:-524288}" ]; then
        mv "$f" "${f}.1"
        > "$f"
    fi
}


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

