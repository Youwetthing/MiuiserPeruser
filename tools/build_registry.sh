#!/data/data/com.termux/files/usr/bin/bash

ROOT="$HOME/MiuiserPeruser"
REG="$ROOT/Registry/system_registry.json"

echo "[+] Building registry..."

# wipe arrays safely
TMP=$(mktemp)

jq '.layers.daemons = [] | .layers.channels = [] | .layers.backends = [] | .layers.failover = []' "$REG" > "$TMP" && mv "$TMP" "$REG"

# DAEMONS
find "$ROOT" -type f -name "*d.log" | while read -r f; do
  jq --arg v "$f" '.layers.daemons += [$v]' "$REG" > "$TMP" && mv "$TMP" "$REG"
done

# LOGS = channels insight layer
find "$ROOT/Log_Cabin" -type f -name "*.log" 2>/dev/null | while read -r f; do
  jq --arg v "$f" '.layers.channels += [$v]' "$REG" > "$TMP" && mv "$TMP" "$REG"
done

# DATABASES
find "$ROOT/Database" -type f 2>/dev/null | while read -r f; do
  jq --arg v "$f" '.layers.backends += [$v]' "$REG" > "$TMP" && mv "$TMP" "$REG"
done

# FAILSAFE (footclan + recovery + blackbox style)
find "$ROOT" -type f \( -name "*blackbox*" -o -name "*zombie*" -o -name "*backup*" \) | while read -r f; do
  jq --arg v "$f" '.layers.failover += [$v]' "$REG" > "$TMP" && mv "$TMP" "$REG"
done

echo "[✓] Registry built."
