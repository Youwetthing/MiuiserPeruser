#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"
EVT="$BASE/state/court.events"

mkdir -p "$BASE/state"

echo "# TIME|SOURCE|EVENT|DETAIL" > "$EVT"

echo "⚖️ Event stream initialised"
