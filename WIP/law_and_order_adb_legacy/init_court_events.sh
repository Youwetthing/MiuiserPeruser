#!/data/data/com.termux/files/usr/bin/bash
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../env.sh"

BASE="$BASE"
EVT="$BASE/state/court.events"

mkdir -p "$BASE/state"


if [ -f "${EVT}" ] && [ "$1" != "--force" ]; then
    echo "⚠️  Already initialised. Use --force to reset."
    exit 1
fi

echo "# TIME|SOURCE|EVENT|DETAIL" > "$EVT"

echo "⚖️ Event stream initialised"
