#!/data/data/com.termux/files/usr/bin/bash
DAEMON="$1"
if [ -z "$DAEMON" ]; then
  echo "Usage: $0 <daemon>"
  exit 1
fi

DB_PATH="/data/data/com.termux/files/home/MiuiserPeruser/logs/syndicate_footclan.db"
LOG_DIR="/data/data/com.termux/files/home/MiuiserPeruser/logs"

echo "[$(date '+%H:%M:%S')] Wrapper starting $DAEMON" >> "$LOG_DIR/wrapper.log"

# Ensure DB
[ -f "$DB_PATH" ] || bash scripts/init_footclan_db.sh

# Launch real daemon
if [ -x "bin/$DAEMON" ]; then
  exec "bin/$DAEMON"
elif [ -x "$DAEMON" ]; then
  exec "./$DAEMON"
else
  echo "ERROR: $DAEMON not found" >> "$LOG_DIR/wrapper.log"
  exit 1
fi
