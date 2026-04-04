#!/bin/bash
PREFIX="/data/data/com.termux/files/usr"
BIN="$PREFIX/bin"
REPO_DIR="$HOME/MiuiserPeruser"

echo "--- MiuiserPeruser: The Syndicate Rises ---"

# 1. Nuanced Rish Setup
if [ ! -f "$BIN/rish" ]; then
    echo "[!] Rish not found in path. Looking for Shizuku assets..."
    # Attempting to pull rish from standard Shizuku export location
    if [ -f "/sdcard/rish" ]; then
        cp /sdcard/rish "$BIN/rish"
        chmod +x "$BIN/rish"
        echo "[✔] Rish installed from /sdcard/."
    else
        echo "[X] Please export 'rish' from the Shizuku app to your Internal Storage first."
        exit 1
    fi
fi

# 2. The Flip Switch Handshake
echo "[*] Initializing Shizuku Bridge..."
if rish -c "id" | grep -q "uid=2000"; then
    echo "[✔] Syndicate has High-Privilege Access (Shell)."
else
    echo "[X] Shizuku denied. Please 'Allow' Termux in the Shizuku app."
    exit 1
fi

# 3. Create the Sewer Pipes
mkdir -p "$REPO_DIR/pipes"
chmod 777 "$REPO_DIR/pipes"

echo "--- Setup Complete. The City is ours. ---"
