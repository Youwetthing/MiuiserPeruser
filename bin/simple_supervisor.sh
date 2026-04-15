#!/data/data/com.termux/files/usr/bin/bash
while true; do
    pgrep -x foot_resurrectord > /dev/null || nohup bin/foot_resurrectord > logs/foot_resurrectord.log 2>&1 &
    pgrep -x foot_ipcshadowd > /dev/null || nohup bin/foot_ipcshadowd > logs/foot_ipcshadowd.log 2>&1 &
    pgrep -x foot_portwatchd > /dev/null || nohup bin/foot_portwatchd > logs/foot_portwatchd.log 2>&1 &
    sleep 12
done
