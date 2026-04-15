#!/data/data/com.termux/files/usr/bin/bash
# Wrapper to ensure DB init and basic logging for Foot Clan core

DAEMON="$1"
DB_PATH="/data/data/com.termux/files/home/MiuiserPeruser/logs/syndicate_footclan.db"
LOG_DIR="/data/data/com.termux/files/home/MiuiserPeruser/logs"

if [ -z "$DAEMON" ]; then
    echo "Usage: $0 <daemon_name>"
    exit 1
fi

echo "[$(date '+%H:%M:%S')] Starting $DAEMON with DB support" | tee -a "$LOG_DIR/wrapper.log"

# Ensure DB is ready
if [ ! -f "$DB_PATH" ]; then
    echo "Initializing DB for $DAEMON..."
    bash scripts/init_footclan_db.sh
fi

# Launch the actual daemon
if [ -x "bin/$DAEMON" ]; then
    exec "bin/$DAEMON"
elif [ -x "$DAEMON" ]; then
    exec "./$DAEMON"
else
    echo "ERROR: $DAEMON not found"
    exit 1
fi
