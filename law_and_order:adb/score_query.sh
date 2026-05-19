#!/data/data/com.termux/files/usr/bin/bash
# score_query.sh — thin wrapper for april_o_neil.sh to call scored
# Usage: score_query.sh <source> <signal> <weight> [context]
# Output: SOURCE|SCORE|STATE|DELTA
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../env.sh"

SOCK="${BASE}/pipes/scored.sock"
SOURCE="$1"
SIGNAL="$2"
WEIGHT="$3"
CONTEXT="${4:-}"

if [[ ! -S "$SOCK" ]]; then
    echo "${SOURCE}|0|CLEAN|0"
    exit 1
fi

echo "${SOURCE}|${SIGNAL}|${WEIGHT}|${CONTEXT}" | \
    nc -U -q1 "$SOCK" 2>/dev/null || echo "${SOURCE}|0|CLEAN|0"
