#!/bin/bash
echo "[*] Compiling binaries..."
# Add your specific build steps here if needed

echo "[*] Clearing the deck (Killing old ghosts)..."
pkill -f miuiserperuser-daemon 2>/dev/null

# Attempt to connect to local ADB/Shizuku if IP fails
adb connect 127.0.0.1:5555 2>/dev/null

if ! adb devices | grep -q "device$"; then
    echo "⚠️  [WARNING] No device found. Check Shizuku/ADB status!"
fi

echo "[*] Deploying Sysportd to Privileged Zone..."
# Your deployment logic here

echo "[*] Starting Rocksteady Client..."
# Your client start logic here

echo "[*] Testing Pipe..."
# Check if sockets are alive
if [ -S "pipes/turtlecom.sock" ]; then
    echo "✅ Pipe is Hot."
else
    echo "❌ Pipe is Cold."
fi
