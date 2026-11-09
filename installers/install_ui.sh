#!/data/data/com.termux/files/usr/bin/bash

BASE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

chmod +x "$BASE/judicial_controller.sh" 2>/dev/null

echo "✔ UI layer installed"
