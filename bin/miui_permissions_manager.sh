#!/data/data/com.termux/files/usr/bin/bash
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║               Miui Permissions Manager Tool                  ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo "Scanning packages + RAM + permissions... (this takes \~10-15s)"

# Get all MIUI + Xiaomi + system packages
adb shell pm list packages -f 2>/dev/null | grep -E "(com\.miui|com\.xiaomi|android)" | cut -d: -f2 | sort > /tmp/packages.txt

echo ""
echo "Package | RAM Usage | Key Permissions | Recommendation | What it does | Safe?"
echo "──────────────────────────────────────────────────────────────────────────────"

while read -r pkg; do
    # RAM
    ram=$(adb shell dumpsys meminfo "$pkg" 2>/dev/null | grep "TOTAL PSS" | awk '{print $2 "K"}' || echo "N/A")

    # Permissions (simplified - looks for dangerous ones)
    perms=$(adb shell dumpsys package "$pkg" 2>/dev/null | grep -E "android.permission.(READ_PHONE_STATE|ACCESS_FINE_LOCATION|READ_CONTACTS|RECORD_AUDIO|CAMERA|WRITE_EXTERNAL_STORAGE)" | head -6 | tr '\n' ' ' || echo "None suspicious")

    # Simple advice + explanation + warning
    if echo "$pkg" | grep -qE "(com\.miui\.home|com\.miui\.securitycenter|com\.miui\.powerkeeper|android)"; then
        advice="DO NOT DISABLE - Critical system component"
        safe="❌ HIGH RISK"
        desc="Core MIUI / Android system app"
    elif echo "$pkg" | grep -qE "(analytics|cloud|msa|weather2|guardprovider)"; then
        advice="Safe to freeze or disable"
        safe="✅ Safe"
        desc="MIUI telemetry / bloat"
    elif echo "$pkg" | grep -qE "(calculator|notes|gallery|cleanmaster)"; then
        advice="Safe to disable if you don't use it"
        safe="✅ Safe"
        desc="Built-in MIUI app"
    else
        advice="Review and consider appops restrictions"
        safe="⚠️ Check"
        desc="Third-party or unknown"
    fi

    printf "%-35s | %-10s | %-20s | %-25s | %s | %s\n" \
           "${pkg:0:35}" "\( ram" " \){perms:0:20}..." "$advice" "$desc" "$safe"
done < /tmp/packages.txt

echo ""
echo "✅ Scan complete. Copy any package name you want to act on."
echo "Next step: you can toggle this as a full background Daemon if you want."
rm -f /tmp/packages.txt
