#!/bin/bash
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../env.sh"
# --- SYNDICATE HIVEMIND BOOTLOADER ---

echo "🧬 Initializing Lymphatic Hub..."
# Kill any stale sessions
pkill -f syndicate_
pkill -f turtlecomd

# 1. Start the IPC Hub & Brain
~/bin/turtlecomd &
sleep 1

# 2. Launch the Specialist Daemons in the background
# (They will now autonomously use the Toolkit based on our hardcoding)
~/bin/syndicate_powerhouse &

echo "✨ Hivemind Circulating. Newsroom Online."
echo "----------------------------------------"
tail -f $BASE/logs/sentinel.log 2>/dev/null || sleep 2
