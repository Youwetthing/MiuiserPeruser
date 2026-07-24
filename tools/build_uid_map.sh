#!/data/data/com.termux/files/usr/bin/bash

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../lib/miuiserperuser_common.sh"

MAP_FILE="data/uid_package_map.txt"
mkdir -p data

echo "🔍 Building complete UID → Package map..."
run_shell "pm list packages -U" 2>/dev/null | sed -n 's/package:\(.*\) uid:\(.*\)/\2 \1/p' > "$MAP_FILE"

count=$(wc -l < "$MAP_FILE")
echo "✅ Map saved to $MAP_FILE with $count entries"
