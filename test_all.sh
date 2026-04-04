#!/data/data/com.termux/files/usr/bin/bash

echo "=== Running daemon (foreground) ==="
./build/src/daemon/miuiserperuser

echo
echo "=== Running --help ==="
./build/src/daemon/miuiserperuser --help

echo
echo "=== Running --selftest ==="
./build/src/daemon/miuiserperuser --selftest

echo
echo "=== Checking structured logs ==="
./build/src/daemon/miuiserperuser --selftest 2>&1 | grep '"level"' || echo "No structured logs found"

echo
echo "=== Smoke test (5 runs) ==="
for i in {1..5}; do
    echo "--- Run \$i ---"
    ./build/src/daemon/miuiserperuser --selftest
done
