#!/data/data/com.termux/files/usr/bin/bash
ADB=/data/data/com.termux/files/home/.cargo/bin/adb_cli
RISH=/data/data/com.termux/files/home/Rish/rish
STATE=/data/data/com.termux/files/home/MiuiserPeruser/pipes/state
mkdir -p $STATE

# Try adb_cli first, fall back to rish
if $ADB tcp 127.0.0.1:5555 shell "echo ok" > /dev/null 2>&1; then
    $ADB tcp 127.0.0.1:5555 shell "sh /sdcard/rish_probe.sh" > $STATE/rahzerd_all 2>/dev/null
else
    RISH_APPLICATION_ID=com.termux $RISH -c "sh /sdcard/rish_probe.sh" > $STATE/rahzerd_all 2>/dev/null
fi
