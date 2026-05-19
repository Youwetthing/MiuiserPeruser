#!/data/data/com.termux/files/usr/bin/bash
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../env.sh"
# Core Resurrection Test Launcher

PROJECT_DIR="$BASE"
LOG_DIR="$PROJECT_DIR/logs"

cd "$PROJECT_DIR" || exit 1

mkdir -p "$LOG_DIR" pipes

termux-wake-lock
echo "✅ Wake-lock acquired"

echo "🧹 Killing old core daemons..."
pkill -f "foot_resurrectord" || true
pkill -f "foot_ipcshadowd" || true
sleep 2

echo "🚀 Launching Core Resurrection Pair..."

nohup bin/foot_ipcshadowd > "$LOG_DIR/foot_ipcshadowd.log" 2>&1 &
echo "   Shadow started (PID $!)"

sleep 2
nohup bin/foot_resurrectord > "$LOG_DIR/foot_resurrectord.log" 2>&1 &
echo "   King started (PID $!)"

echo ""
echo "Core pair is running."
echo "Test the loop now:"
echo "   pkill -f foot_resurrectord"
echo "   sleep 10"
echo "   ps aux | grep -E 'foot_resurrectord|foot_ipcshadowd'"
echo "   tail -n 30 $LOG_DIR/foot_ipcshadowd.log $LOG_DIR/foot_resurrectord.log"
