#!/data/data/com.termux/files/usr/bin/bash
# lock_selftest.sh — verifies flock singleton-lock contract for a daemon.

set -u
BASE="$HOME/MiuiserPeruser"
PASS=0
FAIL=0

check() {
    if [ "$1" = "0" ]; then
        echo "  PASS: $2"
        PASS=$((PASS+1))
    else
        echo "  FAIL: $2"
        FAIL=$((FAIL+1))
    fi
}

test_daemon() {
    local NAME="$1" BIN="$2" PIDFILE="$3"

    echo "== $NAME =="
    pkill -9 -f "bin/$NAME" 2>/dev/null
    sleep 1

    "$BIN" >/dev/null 2>&1 &
    P1=$!
    sleep 1
    if kill -0 "$P1" 2>/dev/null; then check 0 "cold start acquires lock (pid=$P1)"
    else check 1 "cold start acquires lock"; fi

    "$BIN" >/dev/null 2>&1
    RC=$?
    if kill -0 "$P1" 2>/dev/null; then check 0 "first instance still alive after rejected 2nd start"
    else check 1 "first instance still alive after rejected 2nd start"; fi
    if [ "$RC" -ne 0 ]; then check 0 "second instance exits non-zero (rejected)"
    else check 1 "second instance exits non-zero (rejected)"; fi

    COUNT=$(pgrep -f "bin/$NAME" | wc -l)
    if [ "$COUNT" -eq 1 ]; then check 0 "exactly one $NAME process alive (found $COUNT)"
    else check 1 "exactly one $NAME process alive (found $COUNT)"; fi

    kill -9 "$P1" 2>/dev/null
    sleep 1
    if kill -0 "$P1" 2>/dev/null; then check 1 "crashed instance actually dead"
    else check 0 "crashed instance actually dead"; fi

    "$BIN" >/dev/null 2>&1 &
    P2=$!
    sleep 1
    if kill -0 "$P2" 2>/dev/null; then check 0 "fresh instance acquires lock after crash (pid=$P2, stale pidfile did not block)"
    else check 1 "fresh instance acquires lock after crash"; fi

    kill -TERM "$P2" 2>/dev/null
    sleep 1
    if kill -0 "$P2" 2>/dev/null; then check 1 "clean SIGTERM shutdown exits"
    else check 0 "clean SIGTERM shutdown exits"; fi
    if [ ! -f "$PIDFILE" ]; then check 0 "pidfile removed on clean shutdown"
    else check 1 "pidfile removed on clean shutdown"; fi

    pkill -9 -f "bin/$NAME" 2>/dev/null
    echo
}

cd "$BASE" || exit 1
test_daemon "turtlecomd" "$BASE/bin/turtlecomd" "$BASE/pipes/pids/turtlecomd.pid"
test_daemon "splinterd"  "$BASE/bin/splinterd"  "$BASE/pipes/pids/splinterd.pid"

echo "== Summary =="
echo "PASS=$PASS FAIL=$FAIL"
[ "$FAIL" -eq 0 ]
