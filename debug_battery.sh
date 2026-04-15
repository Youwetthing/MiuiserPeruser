#!/data/data/com.termux/files/usr/bin/bash

echo "=== Diagnostic ==="

# Check rish
echo -n "rish: "
if command -v rish >/dev/null; then
    echo "Found at $(which rish)"
    echo -n "  Test: "
    rish echo ok 2>&1 | head -1
else
    echo "Not found"
fi

# Check adb
echo -n "adb: "
if command -v adb >/dev/null; then
    echo "Found at $(which adb)"
    echo -n "  Test: "
    adb shell echo ok 2>&1 | head -1
else
    echo "Not found"
fi

# Check script
echo "Script: tools/battery_truth.sh"
ls -l tools/battery_truth.sh 2>/dev/null || echo "Missing!"

# Run with trace
echo ""
echo "=== Running with trace (first 30 lines) ==="
bash -x tools/battery_truth.sh 2>&1 | head -30
