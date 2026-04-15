#!/data/data/com.termux/files/usr/bin/bash
# Final: Only ONE of each daemon. Older wins. Strict sequence.

LOG="logs/supervisor.log"
echo "[Supervisor] Started at $(date) - ONLY ONE OF EACH" > "$LOG"

# Initial launch in strict sequence
echo "[Supervisor] Launching in sequence (Shadow -> King -> Portwatch)..." | tee -a "$LOG"
nohup bin/foot_ipcshadowd > logs/foot_ipcshadowd.log 2>&1 &
sleep 6
nohup bin/foot_resurrectord > logs/foot_resurrectord.log 2>&1 &
sleep 6
nohup bin/foot_portwatchd > logs/foot_portwatchd.log 2>&1 &
sleep 6

while true; do
    echo "[$(date '+%H:%M:%S')] Enforcing ONE of each..." >> "$LOG"

    # KING - Only one, older wins
    if [ $(pgrep -x foot_resurrectord | wc -l) -gt 1 ]; then
        oldest=$(pgrep -x foot_resurrectord | head -1)
        echo "[Supervisor] Multiple Kings - killing newer, keeping oldest $oldest" | tee -a "$LOG"
        for pid in $(pgrep -x foot_resurrectord); do
            if [ "$pid" != "$oldest" ]; then
                kill -9 "$pid" 2>/dev/null
            fi
        done
    fi

    # SHADOW - Only one, older wins
    if [ $(pgrep -x foot_ipcshadowd | wc -l) -gt 1 ]; then
        oldest=$(pgrep -x foot_ipcshadowd | head -1)
        echo "[Supervisor] Multiple Shadows - killing newer, keeping oldest $oldest" | tee -a "$LOG"
        for pid in $(pgrep -x foot_ipcshadowd); do
            if [ "$pid" != "$oldest" ]; then
                kill -9 "$pid" 2>/dev/null
            fi
        done
    fi

    # PORTWATCH - Only one, older wins
    if [ $(pgrep -x foot_portwatchd | wc -l) -gt 1 ]; then
        oldest=$(pgrep -x foot_portwatchd | head -1)
        echo "[Supervisor] Multiple Portwatch - killing newer, keeping oldest $oldest" | tee -a "$LOG"
        for pid in $(pgrep -x foot_portwatchd); do
            if [ "$pid" != "$oldest" ]; then
                kill -9 "$pid" 2>/dev/null
            fi
        done
    fi

    sleep 12
done
