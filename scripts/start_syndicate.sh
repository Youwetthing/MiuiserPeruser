#!/data/data/com.termux/files/usr/bin/bash
# start_syndicate.sh — launch full syndicate fleet
adb connect 127.0.0.1:5555 2>/dev/null

BASE="$HOME/MiuiserPeruser"
BIN="$BASE/bin"
PIDS="$BASE/pipes/pids"
mkdir -p "$PIDS"

# Ensure rish is executable
chmod +x "$HOME/rish" 2>/dev/null

# Start gaveld first
if ! pgrep -x gaveld > /dev/null; then
    "$BIN/gaveld" &
    echo "[syndicate] gaveld started (pid $!)"
    sleep 1
fi

# Start splinterd (infrastructure coordinator) first
if ! pgrep -x splinterd > /dev/null; then
    "$BIN/splinterd" >> "$BASE/logs/splinterd.log" 2>&1 &
    echo "[syndicate] splinterd started (pid $!)"
    sleep 1
fi

# Start daemons
for daemon in burned granitord leatherheadd rocksteadyd bebopd rahzerd ratkingd metalheadd shredderd fugitoidd krangd turtlecomd; do
    if pgrep -x "$daemon" > /dev/null; then
        echo "[syndicate] $daemon already running"
        continue
    fi
    "$BIN/$daemon" >> "$BASE/logs/${daemon}.log" 2>&1 &
    echo "$!" > "$PIDS/${daemon}.pid"
    echo "[syndicate] $daemon started (pid $!)"
    sleep 0.2
done

bash scripts/superhero_gaveld_bridge.sh >> logs/superhero.log 2>&1 &
echo "[syndicate] superhero bridge started (pid $!)"
echo "[syndicate] fleet launched"

# Keepalive watchdog — restart any dead daemons
(
while true; do
    sleep 30
    for daemon in splinterd burned granitord leatherheadd rocksteadyd bebopd rahzerd ratkingd metalheadd shredderd fugitoidd krangd turtlecomd; do
        if ! pgrep -x "$daemon" > /dev/null; then
            "$BIN/$daemon" >> "$BASE/logs/${daemon}.log" 2>&1 &
            echo "[watchdog] restarted $daemon (pid $!)"
        fi
    done
done
) &
echo "[syndicate] watchdog started (pid $!)"
