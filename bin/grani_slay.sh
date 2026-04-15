#!/data/data/com.termux/files/usr/bin/bash
HOME_DIR="$HOME/MiuiserPeruser"
DB_FILE="$HOME_DIR/logs/syndicate_footclan.db"
echo "[!] GRANITOR PURGE STARTED..."
pkill -9 -f "granitord" || true
rm -f /tmp/granitor.pid
if [ -f "$DB_FILE" ]; then
    sqlite3 "$DB_FILE" "UPDATE heartbeats SET status_flag = 0 WHERE daemon_name = 'granitord';"
fi
echo "[+] PURGE COMPLETE"
ps aux | grep -i granitord | grep -v grep || echo "Clean."
