#!/data/data/com.termux/files/usr/bin/bash

ROOT="$HOME/MiuiserPeruser"
REG="$ROOT/Registry/system_registry.json"

echo "[+] Enforcing daemon state..."

# Extract allowed daemons using python (no jq)
ALLOWED=$(python3 - <<PY
import json
with open("$REG") as f:
    data = json.load(f)
for d in data.get("daemons", []):
    print(d)
PY
)

# Convert to regex
ALLOW_REGEX=$(echo "$ALLOWED" | paste -sd '|' -)

echo "[i] Allowed: $ALLOW_REGEX"

# Kill anything NOT allowed (but ignore system processes)
ps -A -o comm= | while read proc; do
    echo "$proc" | grep -Eq "$ALLOW_REGEX" && continue

    case "$proc" in
        bash|sh|zsh|login|sshd|init|systemd|python*|termux*)
            continue
            ;;
    esac

    pkill -9 -f "$proc" 2>/dev/null
done

# Start allowed daemons if not running
for d in $ALLOWED; do
    pgrep -f "$d" >/dev/null && continue

    if [ -x "$ROOT/bin/$d" ]; then
        echo "[+] Starting $d"
        nohup "$ROOT/bin/$d" > "$ROOT/Log_Cabin/$d.log" 2>&1 &
    fi
done

echo "[✓] Enforcement complete"
