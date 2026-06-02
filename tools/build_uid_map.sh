#!/data/data/com.termux/files/usr/bin/bash

# Source the common library for run_shell
source lib/miuiserperuser_common.sh 2>/dev/null || {
    # Fallback if library missing
    run_shell() {
        if [[ -x "$HOME/rish" ]]; then
            "$HOME/rish" -c "$*" 2>/dev/null
        elif command -v adb >/dev/null 2>&1; then
            adb shell "$@" 2>/dev/null
        fi
    }
}

MAP_FILE="data/uid_package_map.txt"
mkdir -p data

echo "🔍 Building complete UID → Package map..."
run_shell "pm list packages -U" 2>/dev/null | sed -n 's/package:\(.*\) uid:\(.*\)/\2 \1/p' > "$MAP_FILE"

count=$(wc -l < "$MAP_FILE")
echo "✅ Map saved to $MAP_FILE with $count entries"
