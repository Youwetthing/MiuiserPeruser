#!/data/data/com.termux/files/usr/bin/bash
DB="$HOME/MiuiserPeruser/logs/syndicate_footclan.db"

echo "=== SYNDICATE OPERATION: MIUISERPERUSER ==="
echo "Status: TOTAL DOMINATION"
echo "-------------------------------------------"

# Pull the Heartbeat of the Daemons
echo "💓 ACTIVE DAEMONS:"
sqlite3 "$DB" "SELECT daemon_name, pid, last_pulse FROM heartbeats WHERE status_flag = 1;"

# Pull recent kills
echo -e "\n🗡️ RECENT KILLS:"
sqlite3 "$DB" "SELECT process_name, outcome, timestamp FROM kill_history ORDER BY id DESC LIMIT 5;"

# Pull Resurrection stats
echo -e "\n🧟 RESURRECTIONS:"
sqlite3 "$DB" "SELECT COUNT(*) FROM resurrections;" | xargs echo "Total Rebirths:"
