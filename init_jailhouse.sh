#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"
JAIL="$BASE/state/jailhouse"
mkdir -p "$JAIL"

echo "# NAME|STATUS|REASON|TIMESTAMP" > "$JAIL/registry"

echo "🧱 Jailhouse initialised"
