#!/data/data/com.termux/files/usr/bin/bash
# Foot Clan Full Launch v12 - Using all existing binaries

PROJECT_DIR="/data/data/com.termux/files/home/MiuiserPeruser"
LOG_DIR="$PROJECT_DIR/logs"
DB_PATH="$PROJECT_DIR/logs/syndicate_footclan.db"

cd "$PROJECT_DIR" || exit 1

mkdir -p "$LOG_DIR" pipes bin
chmod 777 pipes 2>/dev/null || true

command -v sqlite3 >/dev/null || pkg install sqlite -y
[ -f "$DB_PATH" ] || bash scripts/init_footclan_db.sh

termux-wake-lock
echo "✅ Wake-lock acquired"

echo "🧹 Killing old daemons..."
pkill -f "foot_" || true
pkill -f "*d " || true
sleep 2

echo "Copying all available daemons to bin/..."
cp -f foot_* bin/ 2>/dev/null || true
cp -f build/src/daemon/*d bin/ 2>/dev/null || true

echo "🚀 Launching ALL available Foot Clan daemons..."

for bin in bin/foot_* bin/*d; do
    if [ -x "$bin" ]; then
        name=$(basename "$bin")
        echo "Starting $name ..."
        nohup "$bin" > "$LOG_DIR/$name.log" 2>&1 &
        echo "   → PID $!"
        sleep 1.0
    fi
done

echo ""
echo "=== Running daemons now ==="
ps aux | grep -E 'foot_resurrectord|foot_ipcshadowd|foot_portwatchd|shredderd|rocksteadyd|bebopd|krangd' | grep -v grep || echo "None running"

echo ""
echo "=== DB status ==="
sqlite3 "$DB_PATH" "SELECT COUNT(*) as events FROM events;" 2>/dev/null || echo "0 events"
sqlite3 "$DB_PATH" "SELECT COUNT(*) as resurrections FROM resurrections;" 2>/dev/null || echo "0 resurrections"

echo ""
echo "=== Test the resurrection loop ==="
echo "Run these commands:"
echo "   pkill -f foot_resurrectord"
echo "   sleep 10"
echo "   ps aux | grep -E 'foot_resurrectord|foot_ipcshadowd'"
echo "   sqlite3 logs/syndicate_footclan.db \"SELECT * FROM resurrections ORDER BY id DESC LIMIT 5;\""
