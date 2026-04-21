#!/data/data/com.termux/files/usr/bin/bash
sleep 5
BASE="$HOME/MiuiserPeruser"
[ ! -f "$BASE/bin/miuiser.sh" ] && exit 1
bash "$BASE/bin/miuiser.sh" start
