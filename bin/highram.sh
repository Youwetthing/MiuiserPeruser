#!/data/data/com.termux/files/usr/bin/bash
echo "Top RAM users — Full System View via ADB (who's stealing what)"
echo "──────────────────────────────────────────────────────────────"

echo "Memory summary with MIUI focus:"
adb shell dumpsys meminfo 2>/dev/null | grep -E "(Total PSS|MIUI|com.miui|system_server|powerkeeper|analytics|cloud|daemon|securitycenter)" | head -20

echo ""
echo "Live top 15 processes by RAM usage:"
adb shell "top -m 15 -o %MEM -b -n 1" 2>/dev/null | tail -n +7

echo ""
echo "Total RAM usage:"
free -h | grep Mem | awk '{print $3 " used / " $2 " total"}'

echo ""
echo "Currently running MIUI bloat:"
adb shell "pm list packages | grep com.miui" 2>/dev/null | head -20 || echo "None detected right now"
