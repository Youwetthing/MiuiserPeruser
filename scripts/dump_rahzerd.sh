#!/data/data/com.termux/files/usr/bin/bash
ADB=/data/data/com.termux/files/home/.cargo/bin/adb_cli
STATE=/data/data/com.termux/files/home/MiuiserPeruser/pipes/state
mkdir -p $STATE

# Hard timeout on the entire adb session
timeout 10 $ADB tcp 127.0.0.1:5555 shell "sh /sdcard/rish_probe.sh" > $STATE/rahzerd_all 2>/dev/null || {
    echo "[dump_rahzerd] adb_cli timed out or failed" >&2
}
