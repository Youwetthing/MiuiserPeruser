#!/data/data/com.termux/files/usr/bin/bash
PID=$1
MAPS=$(RISH_APPLICATION_ID=com.termux /data/data/com.termux/files/home/Rish/rish -c "cat /proc/$PID/maps 2>/dev/null")
echo "$MAPS" | grep -E '^[0-9a-f]+-[0-9a-f]+ rwxp 00000000 00:00 0' | grep -q . && echo "RWX:found"
echo "$MAPS" | grep -iE 'frida-agent|linjector|xposed' | grep -q . && echo "INJECTOR:found"
echo "$MAPS" | grep ' /data/' | grep -vE 'apk|dex|oat|art|jar' | grep -E 'rwxp|r-xp' | grep -q . && echo "EXECDATA:found"
