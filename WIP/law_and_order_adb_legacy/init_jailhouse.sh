#!/data/data/com.termux/files/usr/bin/bash
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../env.sh"

BASE="$BASE"
JAIL="$BASE/state/jailhouse"
mkdir -p "$JAIL"


if [ -f "${JAIL}" ] && [ "$1" != "--force" ]; then
    echo "⚠️  Already initialised. Use --force to reset."
    exit 1
fi

echo "# NAME|STATUS|REASON|TIMESTAMP" > "$JAIL/registry"

echo "🧱 Jailhouse initialised"
