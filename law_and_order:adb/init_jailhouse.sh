#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"
JAIL="$BASE/state/jailhouse"
mkdir -p "$JAIL"


if [ -f "${JAIL}" ] && [ "$1" != "--force" ]; then
    echo "⚠️  Already initialised. Use --force to reset."
    exit 1
fi

echo "# NAME|STATUS|REASON|TIMESTAMP" > "$JAIL/registry"

echo "🧱 Jailhouse initialised"
