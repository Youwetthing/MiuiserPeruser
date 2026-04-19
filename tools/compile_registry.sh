#!/data/data/com.termux/files/usr/bin/bash

set -e

ROOT="$HOME/MiuiserPeruser"
ALLOW="$ROOT/Registry/daemon_allowlist.json"
ALIAS="$ROOT/Registry/daemon_aliases.map"
OUT="$ROOT/Registry/system_registry.json"

echo "[+] Compiling daemon registry..."

python3 - <<PY
import json
import os

root = os.path.expanduser("~/MiuiserPeruser")

allow_path = os.path.join(root, "Registry/daemon_allowlist.json")
alias_path = os.path.join(root, "Registry/daemon_aliases.map")
out_path = os.path.join(root, "Registry/system_registry.json")

# Load allowlist
with open(allow_path) as f:
    allow = json.load(f)

allowed = allow if isinstance(allow, list) else []

# Load alias map
alias = {}
if os.path.exists(alias_path):
    with open(alias_path) as f:
        for line in f:
            line = line.strip()
            if "=>" in line and not line.startswith("#"):
                a,b = [x.strip() for x in line.split("=>")]
                alias[a] = b

def resolve(name):
    # apply alias mapping
    return alias.get(name, name)

daemons = [resolve(d) for d in allowed]

# dedupe while preserving order
seen = set()
clean = []
for d in daemons:
    if d not in seen:
        seen.add(d)
        clean.append(d)

registry = {
    "version": "1.0",
    "daemons": clean,
    "governance": ["turtlepower", "judicial_controller"],
    "notes": "auto-generated registry (rahzerd unified layer active)"
}

with open(out_path, "w") as f:
    json.dump(registry, f, indent=2)

print("[✓] Registry compiled successfully")
PY

echo "[✓] Done -> $OUT"
