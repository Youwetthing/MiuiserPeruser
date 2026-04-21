#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"
FLIP="python3 $BASE/flip_switch.py"
PING="$BASE/bin/dex_ping"
TURTLE_SOCK="$BASE/pipes/turtlecom.sock"

pass=0
fail=0

check() {
    local desc="$1"
    local result="$2"
    if [ "$result" = "ok" ]; then
        echo "  [PASS] $desc"
        pass=$((pass + 1))
    else
        echo "  [FAIL] $desc"
        fail=$((fail + 1))
    fi
}

echo ""
echo "[TEST] MiuiserPeruser stack test"
echo "================================"

# 1. Restart clean
echo ""
echo "[TEST] Restarting stack..."
miuiser restart
sleep 3

# 2. Socket checks
[ -S "$BASE/pipes/krang.sock" ]    && r="ok" || r="fail"
check "krang.sock exists"    "$r"
[ -S "$TURTLE_SOCK" ]              && r="ok" || r="fail"
check "turtlecom.sock exists" "$r"

# 3. Chain ping
if [ -x "$PING" ]; then
    "$PING" "$TURTLE_SOCK" > /dev/null 2>&1 && r="ok" || r="fail"
    check "dex_ping chain (dex → turtle → krang)" "$r"
fi

# 4. Verbose logging
echo ""
echo "[TEST] Setting LOG_LEVEL=VERBOSE..."
$FLIP set LOG_LEVEL VERBOSE > /dev/null 2>&1 && r="ok" || r="fail"
check "flip LOG_LEVEL VERBOSE" "$r"

# 5. System lock
echo "[TEST] Locking system..."
$FLIP set SYSTEM_LOCK LOCKED > /dev/null 2>&1 && r="ok" || r="fail"
check "flip SYSTEM_LOCK LOCKED" "$r"

# 6. Ping under lock — expect failure response, not a crash
if [ -x "$PING" ]; then
    resp=$("$PING" "$TURTLE_SOCK" 2>&1)
    echo "$resp" | grep -q "error:system_locked" && r="ok" || r="fail"
    check "ping rejected under SYSTEM_LOCK" "$r"
fi

# 7. Restore
echo ""
echo "[TEST] Restoring defaults..."
$FLIP reset > /dev/null 2>&1 && r="ok" || r="fail"
check "flip reset to defaults" "$r"

# Result
echo ""
echo "================================"
echo "[TEST] Results: $pass passed, $fail failed"
echo ""

[ $fail -eq 0 ] && exit 0 || exit 1
