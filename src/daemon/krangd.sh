#!/bin/bash
# KRANGD: The Key-Master (Environment & Permissions)

SEWER="/data/data/com.termux/files/home/.syndicate_sewer"

while true; do
    # Check for Android Storage Access
    [ -d /sdcard/Android ] && AUTH=1 || AUTH=0
    
    # Check for Termux-Sudo or High-Level execution
    [[ -x $(command -v su) ]] && ROOT=1 || ROOT=0

    # Write the Packet
    echo "auth=$AUTH|root=$ROOT|env=stable" > "$SEWER/krangd.packet.tmp"
    mv "$SEWER/krangd.packet.tmp" "$SEWER/krangd.packet"

    sleep 30
done
