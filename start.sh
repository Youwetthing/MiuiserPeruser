#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"
PIPES="$BASE/pipes"
LOGS="$BASE/Log_Cabin"
KRANG_SOCK="$PIPES/krang.sock"
TURTLE_SOCK="$PIPES/turtlecom.sock"
KRANG_BIN="$BASE/bin/krangd"
TURTLE_BIN="$BASE/bin/turtlecomd"
DEX_PING="$BASE/bin/dex_ping"
POLL_INTERVAL=1
KRANG_TIMEOUT=30

[ ! -d "$PIPES" ] && echo "[START] FATAL: pipes/ missing. Run install.sh first." && exit 1
[ ! -x "$KRANG_BIN" ]  && echo "[START] FATAL: krangd binary not found." && exit 1
[ ! -x "$TURTLE_BIN" ] && echo "[START] FATAL: turtlecomd binary not found." && exit 1

pkill -f krangd 2>/dev/null; pkill -f turtlecomd 2>/dev/null
sleep 1
rm -f "$KRANG_SOCK" "$TURTLE_SOCK"

echo "[START] Starting krangd..."
"$KRANG_BIN" >> "$LOGS/krangd.log" 2>&1 &
KRANG_PID=$!

echo "[START] Waiting for krang.sock..."
elapsed=0
while [ ! -S "$KRANG_SOCK" ]; do
    [ $elapsed -ge $KRANG_TIMEOUT ] && echo "[START] FATAL: krangd timeout." && exit 1
    sleep $POLL_INTERVAL; elapsed=$((elapsed + POLL_INTERVAL))
done
echo "[START] krangd online (${elapsed}s)."

echo "[START] Starting turtlecomd..."
"$TURTLE_BIN" >> "$LOGS/turtlecomd.log" 2>&1 &
TURTLE_PID=$!

elapsed=0
while [ ! -S "$TURTLE_SOCK" ]; do
    [ $elapsed -ge $KRANG_TIMEOUT ] && echo "[START] FATAL: turtlecomd timeout." && exit 1
    sleep $POLL_INTERVAL; elapsed=$((elapsed + POLL_INTERVAL))
done
echo "[START] turtlecomd online (${elapsed}s)."

if [ -x "$DEX_PING" ]; then
    echo "[START] Testing DEX → turtlecom → krang chain..."
    if "$DEX_PING" "$TURTLE_SOCK"; then
        echo "[START] Chain confirmed. System is live."
    else
        echo "[START] WARNING: Chain ping failed. Check logs."
    fi
fi

echo ""
echo "  krangd     PID $KRANG_PID"
echo "  turtlecomd PID $TURTLE_PID"
echo ""
echo "[START] MiuiserPeruser is online. Superhero mode: run superhero.sh when ready."
