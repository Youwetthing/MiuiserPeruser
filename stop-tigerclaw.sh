#!/data/data/com.termux/files/usr/bin/bash
[ -f tigerclaw.pid ] || { echo "No PID file"; exit 1; }
PID=$(cat tigerclaw.pid)
kill "$PID" 2>/dev/null || echo "Process not running"
rm -f tigerclaw.pid
