#!/data/data/com.termux/files/usr/bin/bash
echo "[Runner] Started at $(date)" > logs/runner.log

while true; do
    echo "[$(date '+%H:%M:%S')] Running check..." >> logs/runner.log

    # King
    if ! ps -e | grep -q "[f]oot_resurrectord"; then
        echo "[Runner] Starting King..." | tee -a logs/runner.log
        nohup bin/foot_resurrectord > logs/foot_resurrectord.log 2>&1 &
        sleep 2
    fi

    # Shadow
    if ! ps -e | grep -q "[f]oot_ipcshadowd"; then
        echo "[Runner] Starting Shadow..." | tee -a logs/runner.log
        nohup bin/foot_ipcshadowd > logs/foot_ipcshadowd.log 2>&1 &
        sleep 2
    fi

    # Portwatch
    if ! ps -e | grep -q "[f]oot_portwatchd"; then
        echo "[Runner] Starting Portwatch..." | tee -a logs/runner.log
        nohup bin/foot_portwatchd > logs/foot_portwatchd.log 2>&1 &
        sleep 2
    fi

    sleep 15
done
