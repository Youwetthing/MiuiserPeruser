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


set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

script="$ROOT/scripts/start_footclan.sh"
LOG="$ROOT/logs/start.log"

mkdir -p "$ROOT/logs"

nohup bash "$script" >> "$LOG" 2>&1 &
pid=$!

echo "$pid" > "$ROOT/pidfile"
echo "[OK] started pid=$pid"
