#!/data/data/com.termux/files/usr/bin/bash
ADB=/data/data/com.termux/files/home/.cargo/bin/adb_cli
STATE=/data/data/com.termux/files/home/MiuiserPeruser/pipes/state
mkdir -p $STATE

# adb_cli primary — no rish fallback (rish killed by battery optimisation)
$ADB tcp 127.0.0.1:5555 shell "sh /sdcard/rish_probe.sh" > $STATE/rahzerd_all 2>/dev/null
