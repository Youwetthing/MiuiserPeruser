#!/data/data/com.termux/files/usr/bin/bash
echo "🔍 Discovering MIUI hidden activities..."
tmp="activities_found.txt"
> "$tmp"
packages=$(~/.shizuku/rish -c "pm list packages" 2>/dev/null | sed 's/package://' | grep -E "com\.(miui|xiaomi|android\.settings)")
total=$(echo "$packages" | wc -l)
count=0
echo "$packages" | while read pkg; do
    count=$((count + 1))
    echo -ne "\r   Scanning $count/$total: $pkg...                    "
    ~/.shizuku/rish -c "dumpsys package $pkg" 2>/dev/null | grep -E "$pkg/\." | grep -v "filter" | grep -v "Permission" | head -3 >> "$tmp"
done
echo ""
echo "✅ Found $(wc -l < "$tmp") activities. Showing interesting ones:"
grep -E "Test|Debug|Hardware|Cit|Thermal|Display|Touch|Battery|Network|Radio|Band|Engineer|Factory" "$tmp" | head -25
