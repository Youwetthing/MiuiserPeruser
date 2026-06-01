#!/data/data/com.termux/files/usr/bin/bash
termux-wake-lock
sleep 10

# Connect ADB with retry
for i in 1 2 3; do
    adb connect 127.0.0.1:5555 2>/dev/null
    sleep 2
    adb shell echo ok 2>/dev/null && break
    echo "ADB retry $i" >> ~/logs/boot.log
done

# Find and start Shizuku
SHIZUKU_SO=$(adb shell find /data/app -name "libshizuku.so" 2>/dev/null | head -1 | tr -d '\r')
if [ -n "$SHIZUKU_SO" ]; then
    adb shell "$SHIZUKU_SO" 2>/dev/null
    sleep 3
else
    echo "$(date): Shizuku not found" >> ~/logs/boot.log
fi

chmod +x ~/rish 2>/dev/null
sleep 5

cd ~/MiuiserPeruser
mkdir -p logs pipes/pids
bash scripts/start_syndicate.sh >> logs/boot.log 2>&1
echo "$(date): boot complete" >> ~/logs/boot.log
