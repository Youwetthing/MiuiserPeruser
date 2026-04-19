#!/data/data/com.termux/files/usr/bin/bash
while true; do
    while IFS='|' read -r pkg action ts; do
        if [[ -n "$pkg" ]]; then
            # Redirecting stderr (2) to /dev/null makes the terminal silent
            adb shell cmd appops set "$pkg" RUN_IN_BACKGROUND ignore < /dev/null 2>/dev/null
            adb shell am force-stop "$pkg" < /dev/null >/dev/null 2>&1
            echo "[$(date +%T)] ❄️ [STASIS] Neutered: $pkg" >> ~/MiuiserPeruser/logs/sentinel.log
        fi
    done < ~/MiuiserPeruser/data/daemonhunter_brain.txt
    sleep 20
done
