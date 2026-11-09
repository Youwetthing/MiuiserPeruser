#!/data/data/com.termux/files/usr/bin/bash

BASE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

chmod +x "$BASE/cre/april_o_neil.sh" 2>/dev/null
chmod +x "$BASE/turtlepower_daemon.sh" 2>/dev/null
chmod +x "$BASE/escalation_daemon.sh" 2>/dev/null

echo "✔ Daemons installed"
