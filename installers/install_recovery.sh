#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"

mkdir -p "$BASE/logs"

echo "✔ Recovery layer ready"

# Fold in the analog_init.sh auto-launch hook so turtlecomd comes up
# automatically on new Termux sessions, without clobbering an existing .bashrc.
BASHRC="$HOME/.bashrc"
HOOK_LINE='[ -z "$MP_ON_AIR" ] && [ -f "$HOME/MiuiserPeruser/scripts/install/analog_init.sh" ] && . "$HOME/MiuiserPeruser/scripts/install/analog_init.sh"'

if [ -f "$BASHRC" ] && grep -qF "analog_init.sh" "$BASHRC"; then
    echo "✔ .bashrc hook already present, skipping"
else
    echo "$HOOK_LINE" >> "$BASHRC"
    echo "✔ .bashrc hook installed"
fi

