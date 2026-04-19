#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"
REG="$BASE/state/court.registry"

mkdir -p "$BASE/state"

# reset registry
echo "# NAME|STATE|PID" > "$REG"

echo "⚖️ Court registry initialised"
