#!/data/data/com.termux/files/usr/bin/bash
# Simple one-shot launch with variables

PROJECT_DIR="/data/data/com.termux/files/home/MiuiserPeruser"
LOG_DIR="$PROJECT_DIR/logs"
BIN_DIR="$PROJECT_DIR/bin"

cd "$PROJECT_DIR" || exit 1

echo "=== One-Shot Launch with Variables ==="

# Kill everything first
pkill -9 -f "foot_" || true
sleep 3

# Variables
SHADOW="$BIN_DIR/foot_ipcshadowd"
KING="$BIN_DIR/foot_resurrectord"
PORT="$BIN_DIR/foot_portwatchd"

echo "Starting Shadow..."
nohup "$SHADOW" > "$LOG_DIR/foot_ipcshadowd.log" 2>&1 &
sleep 4

echo "Starting King..."
nohup "$KING" > "$LOG_DIR/foot_resurrectord.log" 2>&1 &
sleep 4

echo "Starting Portwatch..."
nohup "$PORT" > "$LOG_DIR/foot_portwatchd.log" 2>&1 &
sleep 4

echo ""
echo "=== Status ==="
ps aux | grep -E 'foot_ipcshadowd|foot_resurrectord|foot_portwatchd' | grep -v grep

echo ""
echo "=== Logs ==="
tail -n 12 "$LOG_DIR"/foot_ipcshadowd.log "$LOG_DIR"/foot_resurrectord.log "$LOG_DIR"/foot_portwatchd.log 2>/dev/null | cat
