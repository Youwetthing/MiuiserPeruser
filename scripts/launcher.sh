#!/data/data/com.termux/files/usr/bin/bash
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../env.sh"

set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

script="$ROOT/scripts/start_footclan.sh"
LOG="$ROOT/logs/start.log"

mkdir -p "$ROOT/logs"

nohup bash "$script" >> "$LOG" 2>&1 &
pid=$!

echo "$pid" > "$ROOT/pidfile"
echo "[OK] started pid=$pid"
