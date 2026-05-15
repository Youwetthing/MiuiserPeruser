#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"
REG="$BASE/state/court.registry"

mkdir -p "$BASE/state"

# reset registry

if [ -f "${REG}" ] && [ "$1" != "--force" ]; then
    echo "⚠️  Already initialised. Use --force to reset."
    exit 1
fi

echo "# NAME|STATE|PID" > "$REG"

echo "⚖️ Court registry initialised"
